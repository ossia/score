// OPEN-4 -- the incremental-update ordering invariant: every renderer (and
// its render targets) exists BEFORE any pass is created.
//
// NodeRenderer::init() is pure virtual, but the incremental path deliberately
// never calls it: GfxContext::incrementalEdgeUpdate runs
// Graph::reconcileAllRenderLists() -- which creates missing renderers and
// calls ONLY initState() on them -- and only then
// Graph::createAllMissingPasses() -> createPassForEdgeIfMissing(), which
// builds the per-edge passes against the render targets reconcile just
// guaranteed. A pass created against a node whose renderer does not exist yet
// is silently skipped (renderedNodes lookup fails), so an inverted order does
// not crash: it produces a node that renders nothing until some later frame
// happens to sweep again -- which is why the ordering needs a pin and not a
// crash test.
//
// Observable: two SCORE_GFX_TRACE lines at the decision points --
//     GFX-RECONCILE initState node=... rl=...   (renderer created, initState)
//     GFX-PASS add src=... sink=... rl=...      (pass about to be created)
// The pin drives a live filter-style insertion (a NEW producer node wired to
// an existing sink in one edge frame) and asserts that, within that frame,
// every RECONCILE line precedes the first PASS line, and that both exist at
// all -- an inverted order yields no PASS line in the frame, because the new
// node has no renderer when the pass sweep runs.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Gfx.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/ImageNode.hpp>

#include <core/document/Document.hpp>

#include <catch2/catch_test_macros.hpp>

#if defined(__unix__)
#include <unistd.h>
#endif

#include <string>
#include <vector>

namespace
{
#if defined(__unix__)
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

/// Byte offsets of every occurrence of `needle` in `log`.
std::vector<std::size_t> offsetsOf(const std::string& log, const char* needle)
{
  std::vector<std::size_t> r;
  for(auto pos = log.find(needle); pos != std::string::npos;
      pos = log.find(needle, pos + 1))
    r.push_back(pos);
  return r;
}
#endif
}

#if defined(__unix__)
TEST_CASE(
    "all renderers exist before any pass is created in an incremental frame",
    "[gfx][l3][incremental][ordering][gui]")
{
  qputenv("SCORE_GFX_TRACE", "1");

  std::string frame2;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& plug = doc->context().plugin<Gfx::DocumentPlugin>();
    auto& g = plug.context;
    auto& exec = plug.exec;

    auto prodA = std::make_unique<score::gfx::ImagesNode>(doc->context());
    auto prodB = std::make_unique<score::gfx::ImagesNode>(doc->context());
    auto sink = std::make_unique<score::gfx::BackgroundNode>();
    sink->shared_readback = std::make_shared<QRhiReadbackResult>();

    // A two-image-input ISF filter between the producers and the sink, so the
    // frame-2 insertion targets a plain filter node, never an output.
    auto filt = score::test::gfx::make_isf_node(
        QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-mix-two.fs"));
    REQUIRE(filt.node != nullptr);

    // EdgeSpec speaks RAW port indices; resolve the filter's image ports.
    const auto rawIn = [](score::gfx::Node& n, int k) -> int32_t {
      auto* p = score::test::gfx::nth_image_input(n, k);
      REQUIRE(p != nullptr);
      for(std::size_t i = 0; i < n.input.size(); i++)
        if(n.input[i] == p)
          return int32_t(i);
      FAIL("image input not in the node's input list");
      return -1;
    };
    auto& fref = *filt.node;
    const int32_t f_in0 = rawIn(fref, 0);
    const int32_t f_in1 = rawIn(fref, 1);

    const int32_t a = g.register_node(std::move(prodA));
    const int32_t f = g.register_node(std::move(filt.node));
    const int32_t s = g.register_node(std::move(sink));

    const auto publish = [&](std::vector<Gfx::EdgeSpec> es) {
      ossia::audio_tick_state st{};
      exec.startTick(st);
      for(const auto& e : es)
        exec.setEdge(e.first, e.second, e.type);
      exec.endTick(st);
    };

    using pi = Gfx::port_index;
    const auto glutton = Process::CableType::ImmediateGlutton;
    const Gfx::EdgeSpec eAF{pi{a, 0}, pi{f, f_in0}, glutton};
    const Gfx::EdgeSpec eFS{pi{f, 0}, pi{s, 0}, glutton};

    // Frame 1: baseline graph A -> filter -> sink; full rebuild (the output
    // node landed in the same frame).
    publish({eAF, eFS});
    g.updateGraph();

    // Frame 2: live insertion of a NEW producer wired to the filter's second
    // image input, in one edge frame -- the exact shape whose ordering is
    // under pin. The producer is not an output, so no full rebuild: this
    // takes the incremental path (reconcile, then passes).
    const int32_t b = g.register_node(std::move(prodB));
    const Gfx::EdgeSpec eBF{pi{b, 0}, pi{f, f_in1}, glutton};
    publish({eAF, eFS, eBF});
    {
      StderrCapture cap;
      g.updateGraph();
      frame2 = cap.finish();
    }

    g.unregister_node(b);
    g.unregister_node(s);
    g.unregister_node(f);
    g.unregister_node(a);
    g.updateGraph();
  });

  INFO(frame2);

  // The frame consumed its edge diff incrementally...
  CHECK(frame2.find("GFX-EDGES consume") != std::string::npos);
  CHECK(frame2.find("full=0") != std::string::npos);

  const auto reconciles = offsetsOf(frame2, "GFX-RECONCILE initState");
  const auto passes = offsetsOf(frame2, "GFX-PASS add");

  // ...created the new node's renderer via initState -- reconcile is the ONLY
  // legal creator on this path (init() must never run here)...
  REQUIRE(!reconciles.empty());

  // ...and created the new edge's pass IN THE SAME FRAME. An inverted order
  // (passes before reconcile) leaves no renderer for the new node when the
  // pass sweep runs, so this list would be empty and the node black.
  REQUIRE(!passes.empty());

  // The invariant itself: every renderer creation precedes every pass
  // creation within the frame.
  CHECK(reconciles.back() < passes.front());
}
#else
TEST_CASE(
    "all renderers exist before any pass is created in an incremental frame")
{
  SKIP("stderr capture via dup2 is a unix-only harness");
}
#endif
