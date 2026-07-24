// =============================================================================
// L3 INCREMENTAL graph-edit — Vulkan resize-after-add regression guard (Prio 5).
//
// HISTORY (fixed engine bug, was a cross-backend divergence):
//   The two cases below, run in that order in the SAME process, used to trip a
//   Vulkan-only use-after-free of a VkRenderPass. The real culprit was NOT the
//   incremental-add path leaking, but the sink RESIZE path: BackgroundNode::
//   resize() deleteLater()-destroys the offscreen sink's QRhiTextureRenderTarget
//   and QRhiRenderPassDescriptor and installs fresh ones, while its
//   InvertYRenderer had cached the OLD target/renderpass by value at
//   createRenderer() time. The resize fast path (resizeSwapchainSizedTargets ->
//   maybeRebuild release()+init()) rebuilt the render list in place WITHOUT
//   reconstructing that renderer, so the upstream node's final pass rebuilt its
//   graphics pipeline against the freed QRhiRenderPassDescriptor:
//       vkCreateGraphicsPipelines(): pCreateInfos[0].renderPass is not a valid
//       render pass  ->  VK_ERROR_VALIDATION_FAILED_EXT (-1000011001) / SIGSEGV.
//   Whether the freed VkRenderPass memory was still intact was heap-state
//   dependent, so the failure was an intermittent (~9/10) RACE. OpenGL has no
//   VkRenderPass object and was always green.
//
//   Fixed by OutputNode::currentRenderTarget() + InvertYRenderer::init()
//   re-adopting the sink's live target on every rebuild (commit "gfx: fix stale
//   VkRenderPass use-after-free on offscreen-sink resize"). Both cases are now
//   GREEN on every backend and this target stays as a regression guard.
//
//   The crash needed TWO ingredients in the same process:
//     (1) the incremental add-of-a-new-output (first TEST_CASE below), and
//     (2) a later render that rebuilds a pipeline — a resize (second TEST_CASE).
//   (1) churns the GPU allocator enough that (2)'s freed-then-rebound renderpass
//   landed on reused memory — which is why (2) is green in isolation but was red
//   after (1). This target is kept ISOLATED (its own executable) so any future
//   regression here (a contained Vulkan crash) cannot abort the rest of the
//   incremental coverage in test_gfx_incremental.
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_incremental_findings   # green
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_incremental_findings   # green
// =============================================================================
#include "GfxIncrementalCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;
using namespace score::test::gfx::incremental;

// (1) The trigger: incremental add-of-a-new-output. Correct on every backend in
//     isolation — asserts the added node's output appears. On Vulkan it also
//     leaks the render pass that case (2) then trips over.
TEST_CASE("FINDING add-new-output incremental (Vulkan leaks a render pass)", "[gfx][l3][incremental][finding]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Shot out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int a = p.addIsf(corpus("isf-solid-color.fs"));
    const int s0 = p.addSink({64, 64});
    const int s1 = p.addSink({64, 64});
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
    const int b = p.addIsf(corpus("isf-solid-color.fs"));
    p.addEdgeIncremental(p.imageOut(b, 0), p.sinkInput(s1));
    p.render(3);
    out.c = p.readback(s1);
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty());
  REQUIRE(out.c.valid());
  CHECK(solid(out.c, {255, 0, 255, 255}, 2)); // the added output appears
}

// (2) The victim: a plain resize-mid-render. CORRECT in isolation (it is exactly
//     the green case in test_gfx_incremental). Here it runs in the SAME process
//     AFTER case (1); on Vulkan the leaked render pass from (1) makes its
//     pipeline rebuild dereference a stale VkRenderPass and SIGSEGV. On OpenGL it
//     passes, proving the divergence is Vulkan-specific.
TEST_CASE("FINDING resize after an incremental add crashes on Vulkan", "[gfx][l3][incremental][finding]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Shot out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int a = p.addIsf(corpus("isf-solid-color.fs"));
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
    p.resizeSink(s0, {96, 48}); // <- Vulkan: SIGSEGV on the stale render pass
    p.render(3);
    p.resizeSink(s0, {40, 40});
    p.render(3);
    out.c = p.readback(s0);
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty());
  REQUIRE(out.c.valid());
  CHECK(out.c.width == 40);
  CHECK(out.c.height == 40);
  CHECK(solid(out.c, {255, 0, 255, 255}, 2));
}
