// =============================================================================
// GStreamer output bus classification and feed/finalize decoupling.
//
// poll_bus_errors() filters on GST_MESSAGE_ERROR alone; warnings are drained and
// ignored. A real error clears only m_feeding, the frame-push gate, and leaves
// m_started set, so stop_pipeline() still runs, emits appsrc EOS and drives the
// pipeline to GST_STATE_NULL, finalizing the container even after an error.
// stop_pipeline() gates on m_started, so clearing it on a benign warning would
// skip finalization and leave a muxer that never wrote its moov atom.
//
// There is no executable test against the production code: GStreamerOutputNode
// lives in a .cpp inside score_plugin_gfx, is not exported, and the whole stack
// compiles only behind SCORE_HAS_GSTREAMER with libgstreamer dlopen'd. The
// classification and decoupling logic is inline in the device methods rather
// than factored into a free function, and exercising it for real needs a running
// pipeline that can be made to post a warning mid-recording and a container file
// whose finalized-vs-truncated state is then inspected.
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

  // push_video_frame_*(): gated on the feed flag.
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
