// =============================================================================
// L3 INCREMENTAL graph-edit — ADD NODE + EDGE mid-render (Priority 5).
//
// A brand-new node is built AFTER the render lists are up (GfxPipeline::addIsf
// registers it in the graph) and wired into a previously-idle sink through the
// SAME incremental path the app uses (GfxPipeline::addEdgeIncremental ->
// Graph::addEdge + reconcileAllRenderLists, which gives the newly-reachable node
// a renderer + passes). The new node's output must then appear at that sink.
//
// ISOLATION: this test lives in its OWN ctest target. On Vulkan the incremental-
// add-of-a-new-output path leaks a VkRenderPass that is only released when the
// whole GfxContext/app is torn down — running a *subsequent* in-process render
// that rebuilds pipelines (a sink resize) then dereferences the stale handle and
// SIGSEGVs. That interaction is captured, isolated and documented in
// test_gfx_incremental_findings (GfxIncrementalFindings.cpp). Keeping this
// coverage test alone in its own process means its Vulkan teardown cannot perturb
// the green resize/reconnect cases in test_gfx_incremental — mirroring the
// isolation convention used by test_gfx_isf_mrt_persistent / test_gfx_csf_image3d.
//
// Run: DISPLAY=:0 ctest -R gfx_incremental_add --output-on-failure
// =============================================================================
#include "GfxIncrementalCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;
using namespace score::test::gfx::incremental;

TEST_CASE("incremental add-node/add-edge: new output appears mid-render", "[gfx][l3][incremental][add]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Shot out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int a = p.addIsf(corpus("isf-solid-color.fs")); // magenta producer
    const int s0 = p.addSink({64, 64});
    const int s1 = p.addSink({64, 64}); // initially unwired -> renders nothing
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
    out.a = p.readback(s0); // s0 magenta
    out.b = p.readback(s1); // s1 empty (no input yet)

    // Mid-render: add a brand-new node B and wire it into the idle sink s1.
    const int b = p.addIsf(corpus("isf-solid-color.fs"));
    if(b < 0)
    {
      out.error = p.error();
      return;
    }
    p.addEdgeIncremental(p.imageOut(b, 0), p.sinkInput(s1));

    p.render(3);
    out.c = p.readback(s1); // s1 must now show B's magenta output
    if(out.a.valid())
      out.a = p.readback(s0); // s0 must be unaffected
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty());

  // Before the edit, s1 rendered nothing (empty/cleared readback).
  INFO("s1 pre-edit valid=" << out.b.valid());
  CHECK(!solid(out.b, {255, 0, 255, 255}, 8));

  // After adding node B + edge, s1 shows B's magenta output; s0 still magenta.
  REQUIRE(out.c.valid());
  CHECK(solid(out.c, {255, 0, 255, 255}, 2));
  CHECK(solid(out.a, {255, 0, 255, 255}, 2));
}
