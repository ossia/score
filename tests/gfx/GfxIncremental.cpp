// =============================================================================
// L3 INCREMENTAL graph-edit path (Priority 5) — the SAFE, green coverage.
//
// Exercises the render engine's ability to edit the graph WHILE rendering,
// driving the SAME incremental machinery the app uses (see GfxIncrementalCommon
// .hpp). Two scenarios that behave correctly and identically on every backend:
//
//   1. resize a sink mid-render        -> no crash, readback at the new size,
//      pixels still correct (the resize P0 class): BackgroundNode::setSize ->
//      resize() -> Graph::recreateOutputRenderList (GPU drain + rebuild the
//      output's RenderList at the new size).
//   2. reconnect after disconnect      -> remove the sole feeding edge (sink goes
//      idle), re-add it (sink renders again), no crash across the churn.
//
// The add-node/add-edge mid-render scenario lives in its own target
// (test_gfx_incremental_add) and its Vulkan resource-lifetime finding in
// test_gfx_incremental_findings — see those files. Both are isolated so their
// Vulkan teardown interaction cannot perturb these green cases.
//
// Every case iterates platform_backends() and asserts per backend.
//
// Run: DISPLAY=:0 ctest -R gfx_incremental --output-on-failure
// =============================================================================
#include "GfxIncrementalCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;
using namespace score::test::gfx::incremental;

// -----------------------------------------------------------------------------
// 1. Resize a sink mid-render: no crash, readback at the new size, still correct.
// -----------------------------------------------------------------------------
TEST_CASE("incremental resize sink mid-render: correct output at new size", "[gfx][l3][incremental]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Shot out;
  int newW = 0, newH = 0;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int a = p.addIsf(corpus("isf-solid-color.fs")); // magenta
    const int s0 = p.addSink({64, 64});
    p.wire(p.imageOut(a, 0), p.sinkInput(s0));

    if(!p.create(backend))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.backend = p.backend();
      out.error = p.error();
      return;
    }
    out.backend = p.backend();

    p.render(3);
    out.a = p.readback(s0); // 64x64 magenta

    // Grow, then shrink to a non-square size to stress the rebuild both ways.
    p.resizeSink(s0, {96, 48});
    p.render(3);
    out.b = p.readback(s0); // must be 96x48 magenta

    p.resizeSink(s0, {40, 40});
    p.render(3);
    out.c = p.readback(s0); // must be 40x40 magenta
    newW = out.c.width;
    newH = out.c.height;
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty());

  // Initial 64x64.
  REQUIRE(out.a.valid());
  CHECK(out.a.width == 64);
  CHECK(out.a.height == 64);
  CHECK(solid(out.a, {255, 0, 255, 255}, 2));

  // After growing to 96x48: right size, right pixels, no crash.
  REQUIRE(out.b.valid());
  CHECK(out.b.width == 96);
  CHECK(out.b.height == 48);
  CHECK(solid(out.b, {255, 0, 255, 255}, 2));

  // After shrinking to 40x40.
  INFO("final size " << newW << "x" << newH);
  REQUIRE(out.c.valid());
  CHECK(out.c.width == 40);
  CHECK(out.c.height == 40);
  CHECK(solid(out.c, {255, 0, 255, 255}, 2));
}

// -----------------------------------------------------------------------------
// 2. Reconnect after disconnect: remove the sole feeding edge (sink goes idle),
//    then re-add it (sink renders again). No crash across the churn.
// -----------------------------------------------------------------------------
TEST_CASE("incremental reconnect after disconnect", "[gfx][l3][incremental]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Shot out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int a = p.addIsf(corpus("isf-solid-color.fs")); // magenta
    const int s0 = p.addSink({64, 64});
    p.wire(p.imageOut(a, 0), p.sinkInput(s0));

    if(!p.create(backend))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.backend = p.backend();
      out.error = p.error();
      return;
    }
    out.backend = p.backend();

    p.render(3);
    out.a = p.readback(s0); // magenta

    // Disconnect the only edge feeding the sink.
    p.removeEdgeIncremental(p.imageOut(a, 0), p.sinkInput(s0));
    p.render(3);
    out.b = p.readback(s0); // sink idle -> empty/cleared

    // Reconnect.
    p.addEdgeIncremental(p.imageOut(a, 0), p.sinkInput(s0));
    p.render(3);
    out.c = p.readback(s0); // magenta again
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty());

  REQUIRE(out.a.valid());
  CHECK(solid(out.a, {255, 0, 255, 255}, 2));

  // After disconnect the sink no longer renders the producer.
  INFO("post-disconnect valid=" << out.b.valid());
  CHECK(!solid(out.b, {255, 0, 255, 255}, 8));

  // After reconnect the producer's output is back.
  REQUIRE(out.c.valid());
  CHECK(solid(out.c, {255, 0, 255, 255}, 2));
}
