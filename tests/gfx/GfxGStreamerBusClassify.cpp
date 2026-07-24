// =============================================================================
// F9 / R2-#9 — GStreamer output bus classification + feed/finalize decoupling
// (commit 257035968). DOCUMENTED COVERAGE GAP + behavioral MODEL guard.
//
// FINDING: GStreamerOutputNode::poll_bus_errors() popped bus messages filtered
// on GST_MESSAGE_ERROR | GST_MESSAGE_WARNING and, on ANY match, unconditionally
// set m_started=false. m_started is ALSO the flag stop_pipeline() gates on
// (`if(!m_pipeline || !m_started) return;`), so a benign WARNING (routinely
// posted during healthy encoding: dropped/late buffers, missing PTS, encoder
// rate warnings) neutralized finalization: stop_pipeline() returned before
// sending appsrc EOS, and cleanup_pipeline() unref'd a still-PLAYING pipeline
// whose muxer never wrote its moov atom / cluster index -> truncated, unplayable
// output file.
//
// THE FIX (two parts, both verified by code reading in
// GStreamerOutputDevice.cpp):
//   1. CLASSIFICATION: poll_bus_errors() now filters on GST_MESSAGE_ERROR alone
//      (bus_timed_pop_filtered(bus, 0, GST_MESSAGE_ERROR)); WARNINGs are drained
//      and ignored, never touching pipeline state.
//   2. DECOUPLING: a real error clears only m_feeding (the frame-push gate) and
//      deliberately LEAVES m_started set, so stop_pipeline() still runs, emits
//      EOS and drives the pipeline to GST_STATE_NULL — finalizing the file even
//      after a genuine error.
//
// ---------------------------------------------------------------------------
// WHY THERE IS NO EXECUTABLE TEST AGAINST THE PRODUCTION CODE (honest gap):
//   * GStreamerOutputNode lives in a .cpp inside score_plugin_gfx, is not an
//     exported symbol, and the whole GStreamer output stack is compiled only
//     behind a build flag (SCORE_HAS_GSTREAMER) with libgstreamer dlopen'd at
//     runtime. None of it is linkable/callable from a unit-test TU.
//   * poll_bus_errors() / stop_pipeline() are device methods that operate on a
//     live GstBus + appsrc/muxer/filesink pipeline. Exercising the real bug
//     needs a running GStreamer pipeline that (a) can be made to post a WARNING
//     mid-recording and (b) writes a container file whose finalized-vs-truncated
//     state is then inspected — a full media harness with a GStreamer install,
//     well beyond a headless unit test, and skipped entirely when the flag/lib
//     is absent.
//   * The classification + decoupling logic is NOT factored into a free/testable
//     function in production (it is inline in the device methods), so there is
//     no real symbol to unit-test. Refactoring production to extract it is out
//     of scope for a tests-only change.
//
//   Per the review methodology this gap is recorded explicitly (never faked
//   GREEN), mirroring the honest SKIP GT1 shipped for R2-#8.
//
// WHAT IS TESTED HERE (a MODEL, clearly labelled):
//   Below is a faithful behavioral MODEL of the fix's decision table — the two
//   flags (m_started/m_feeding) and the gating of poll/stop/push — for both the
//   PRE-fix and POST-fix logic. It is NOT the production code; it encodes the
//   fix's contract as an executable, deterministic proof and documents the exact
//   divergence. It cannot flip RED against the reverted engine (the engine code
//   is unreachable from here); it guards the *intent* and makes the invariants
//   reviewable. The production behavior is verified by code reading (see commit
//   257035968 / GStreamerOutputDevice.cpp lines 316-400).
// =============================================================================
#include <catch2/catch_test_macros.hpp>

namespace
{
// Minimal message kinds mirroring the GstMessageType bits the poller cares about.
enum class Msg
{
  Warning,
  Error
};

// Behavioral model of GStreamerOutputNode's frame/finalize state machine.
// `preFix == true` reproduces the buggy single-flag logic; `false` the fix.
struct OutputModel
{
  bool m_started = false;
  bool m_feeding = false;
  bool preFix = false;

  // Bookkeeping to observe finalization behavior.
  bool eos_emitted = false;      // stop_pipeline() reached the appsrc EOS send
  bool set_null = false;         // pipeline driven to GST_STATE_NULL
  int frames_pushed = 0;         // push_video_frame_* actually pushed

  void start_pipeline()
  {
    m_started = true;
    m_feeding = true;
  }

  // poll_bus_errors(): react to a single bus message.
  void poll(Msg m)
  {
    if(preFix)
    {
      // Pre-fix: filter matched ERROR *and* WARNING, and any match killed
      // m_started (which also gates finalization).
      if(m == Msg::Error || m == Msg::Warning)
        m_started = false;
    }
    else
    {
      // Fix: only a genuine ERROR is fatal; it stops feeding but leaves
      // m_started set so stop_pipeline() can still finalize. WARNING ignored.
      if(m == Msg::Error)
        m_feeding = false;
      // WARNING: drained, no state change.
    }
  }

  // push_video_frame_*(): gated on the feed flag (m_started pre-fix, m_feeding
  // post-fix — matching the production guards).
  void push_frame()
  {
    const bool gate = preFix ? m_started : m_feeding;
    if(gate)
      ++frames_pushed;
  }

  // stop_pipeline(): gated on m_started in BOTH versions. Emits EOS then NULL.
  void stop_pipeline()
  {
    if(!m_started)
      return;
    eos_emitted = true; // appsrc EOS -> muxer finalizes moov atom / cluster idx
    set_null = true;
    m_started = false;
    m_feeding = false;
  }
};
} // namespace

TEST_CASE(
    "F9 model: a bus WARNING does not neutralize finalization (fix)",
    "[gfx][unit][gstreamer][bus][model]")
{
  OutputModel dev;
  dev.preFix = false;
  dev.start_pipeline();

  // A benign WARNING arrives mid-recording (common during healthy encoding).
  dev.poll(Msg::Warning);

  // Feeding continues and the pipeline is still live/finalizable.
  CHECK(dev.m_feeding == true);
  CHECK(dev.m_started == true);
  dev.push_frame();
  CHECK(dev.frames_pushed == 1);

  // On user stop, EOS is emitted and the file is finalized.
  dev.stop_pipeline();
  CHECK(dev.eos_emitted == true);
  CHECK(dev.set_null == true);
}

TEST_CASE(
    "F9 model: a genuine ERROR stops feeding but STILL finalizes (fix)",
    "[gfx][unit][gstreamer][bus][model]")
{
  OutputModel dev;
  dev.preFix = false;
  dev.start_pipeline();

  dev.poll(Msg::Error);

  // Frame feed is gated off (m_feeding=false)...
  CHECK(dev.m_feeding == false);
  dev.push_frame();
  CHECK(dev.frames_pushed == 0);

  // ...but m_started stays set so stop_pipeline() still emits EOS -> the file
  // is finalized rather than truncated (the key decoupling of the fix).
  CHECK(dev.m_started == true);
  dev.stop_pipeline();
  CHECK(dev.eos_emitted == true);
  CHECK(dev.set_null == true);
}

TEST_CASE(
    "F9 model: PRE-fix logic truncates the file on a WARNING (documents the bug)",
    "[gfx][unit][gstreamer][bus][model]")
{
  OutputModel dev;
  dev.preFix = true;
  dev.start_pipeline();

  // The pre-fix poller treats a WARNING as fatal and clears m_started.
  dev.poll(Msg::Warning);
  CHECK(dev.m_started == false);

  // All further frames are silently dropped...
  dev.push_frame();
  CHECK(dev.frames_pushed == 0);

  // ...and stop_pipeline() early-returns WITHOUT emitting EOS: the muxer never
  // finalizes -> truncated, unplayable file. This is exactly the defect the fix
  // removes (contrast the two model cases above).
  dev.stop_pipeline();
  CHECK(dev.eos_emitted == false);
  CHECK(dev.set_null == false);
}

TEST_CASE(
    "F9 production coverage — documented gap (real device not headlessly testable)",
    "[gfx][unit][gstreamer][bus][skip]")
{
  SKIP("F9/R2-#9 (GStreamer bus WARNING-vs-ERROR classification + m_feeding/"
       "m_started decoupling) is NOT unit-testable against production code: "
       "GStreamerOutputNode is an unexported struct in a .cpp compiled only "
       "behind SCORE_HAS_GSTREAMER with libgstreamer dlopen'd at runtime, and "
       "the classification logic is inline in device methods (no free function "
       "to call). Exercising it needs a live GStreamer pipeline made to post a "
       "WARNING mid-recording and a finalized-vs-truncated container-file check "
       "— a full media harness, not a headless unit test. The fix is verified by "
       "code reading (commit 257035968) and its contract is guarded by the "
       "behavioral MODEL cases in this file. See the file header for the full "
       "rationale.");
}
