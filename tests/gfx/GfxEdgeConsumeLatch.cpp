// OPEN-3 -- the full-rebuild latch must not outlive its frame.
//
// GfxContext::updateGraph runs two channels: node COMMANDS (run_commands,
// which sets m_fullRebuildThisFrame when a command forced a full rebuild) and
// the EDGE set (edges_changed + new_edges, consumed later in the same call).
// When a full rebuild runs in a frame with NO edge change pending, the flag
// used to latch: the next edge publication -- however small -- was then
// downgraded into another full rebuild, silently turning every later
// incremental diff into a from-scratch reconstruction. GfxContext.cpp now
// clears the flag right after run_commands when no edge change is pending;
// nothing drove that clear path.
//
// This drives it with the REAL document GfxContext (Gfx::DocumentPlugin's),
// through the real producer channel (GfxExecutionAction::setEdge/endTick),
// with updateGraph called synchronously on the UI thread -- the same thread
// its timers would call it on; no events are pumped, so nothing interleaves.
//
// The observable is the one the engine provides: SCORE_GFX_TRACE prints
//     GFX-EDGES consume old=<n> new=<n> full=<0|1>
// at the exact decision point. stderr is captured around each updateGraph.
//
//   frame 1: both nodes land (the sink is an output -> full rebuild) AND the
//            first edge set is pending -> consume full=1. Baseline, proves
//            the flag is visible in the trace at all.
//   frame 2: a second output node lands, NO edge change -> full rebuild ran,
//            flag set, and the clear-outside-the-branch is what must reset it.
//   frame 3: one added edge -> consume must say full=0 and take the
//            incremental path. With the latch (pre-fix), this says full=1.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/ImageNode.hpp>
#include <Gfx/Graph/OutputNode.hpp>

#include <core/document/Document.hpp>

#include <catch2/catch_test_macros.hpp>

#if defined(__unix__)
#include <unistd.h>
#endif

#include <regex>
#include <string>

namespace
{
#if defined(__unix__)
/// Captures everything written to fd 2 (fprintf(stderr, ...) included, which
/// qInstallMessageHandler would not see) between construction and finish().
struct StderrCapture
{
  int saved{-1};
  int fds[2]{-1, -1};

  StderrCapture()
  {
    ::fflush(stderr);
    saved = ::dup(2);
    ::pipe(fds);
    ::dup2(fds[1], 2);
    ::close(fds[1]);
  }

  std::string finish()
  {
    ::fflush(stderr);
    ::dup2(saved, 2);
    ::close(saved);
    std::string r;
    char buf[4096];
    ssize_t n;
    while((n = ::read(fds[0], buf, sizeof buf)) > 0)
      r.append(buf, std::size_t(n));
    ::close(fds[0]);
    return r;
  }
};

/// The full=%d of the last "GFX-EDGES consume" line, or -1 when none printed.
int lastConsumeFull(const std::string& log)
{
  static const std::regex re{"GFX-EDGES consume old=\\d+ new=\\d+ full=(\\d)"};
  int full = -1;
  for(auto it = std::sregex_iterator(log.begin(), log.end(), re);
      it != std::sregex_iterator(); ++it)
    full = (*it)[1].str() == "1" ? 1 : 0;
  return full;
}
#endif
}

#if defined(__unix__)
TEST_CASE(
    "a full rebuild with no edge change does not downgrade the next "
    "incremental diff",
    "[gfx][l3][incremental][latch][gui]")
{
  qputenv("SCORE_GFX_TRACE", "1");

  // Collected inside the lambda, asserted after teardown, per the fixture doc.
  int frame1Full = -2, frame2Full = -2, frame3Full = -2;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& plug = doc->context().plugin<Gfx::DocumentPlugin>();
    auto& g = plug.context;
    auto& exec = plug.exec;

    // A producer and an offscreen sink. The sink is an OutputNode, so its
    // ADD_NODE forces the full rebuild of frame 1.
    auto prod = std::make_unique<score::gfx::ImagesNode>(doc->context());
    auto sinkA = std::make_unique<score::gfx::BackgroundNode>();
    sinkA->shared_readback = std::make_shared<QRhiReadbackResult>();
    auto sinkB = std::make_unique<score::gfx::BackgroundNode>();
    sinkB->shared_readback = std::make_shared<QRhiReadbackResult>();

    const int32_t a = g.register_node(std::move(prod));
    const int32_t sA = g.register_node(std::move(sinkA));

    // The producer channel, verbatim: every tick re-sends the COMPLETE edge
    // set; endTick publishes it when it differs from the previous tick's.
    const auto publish = [&](std::vector<Gfx::EdgeSpec> es) {
      ossia::audio_tick_state st{};
      exec.startTick(st);
      for(const auto& e : es)
        exec.setEdge(e.first, e.second, e.type);
      exec.endTick(st);
    };

    using pi = Gfx::port_index;
    const Gfx::EdgeSpec eA{pi{a, 0}, pi{sA, 0}, Process::CableType::ImmediateGlutton};

    // Frame 1: nodes + first edge set together.
    publish({eA});
    {
      StderrCapture cap;
      g.updateGraph();
      frame1Full = lastConsumeFull(cap.finish());
    }

    // Frame 2: a command-driven full rebuild with NO edge change pending --
    // the exact shape that used to latch the flag.
    const int32_t sB = g.register_node(std::move(sinkB));
    {
      StderrCapture cap;
      g.updateGraph();
      frame2Full = lastConsumeFull(cap.finish());
    }

    // Frame 3: a one-edge increment. This consume is the assertion.
    const Gfx::EdgeSpec eB{pi{a, 0}, pi{sB, 0}, Process::CableType::ImmediateGlutton};
    publish({eA, eB});
    {
      StderrCapture cap;
      g.updateGraph();
      frame3Full = lastConsumeFull(cap.finish());
    }

    // Leave the graph edge-consistent before teardown.
    g.unregister_node(sB);
    g.unregister_node(sA);
    g.unregister_node(a);
    g.updateGraph();
  });

  // Baseline: the first consume happened in the same frame as the output
  // node's full rebuild, so the trace must show the flag at work.
  CHECK(frame1Full == 1);

  // No edge change was pending in frame 2, so no consume line at all.
  CHECK(frame2Full == -1);

  // The point: frame 2's full rebuild must NOT downgrade frame 3's one-edge
  // diff. A latched flag makes this 1.
  CHECK(frame3Full == 0);
}
#else
TEST_CASE(
    "a full rebuild with no edge change does not downgrade the next "
    "incremental diff")
{
  SKIP("stderr capture via dup2 is a unix-only harness");
}
#endif
