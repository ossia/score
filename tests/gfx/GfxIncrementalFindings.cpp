// =============================================================================
// L3 INCREMENTAL graph-edit -- Vulkan resize-after-add regression guard.
//
// BackgroundNode::resize() deleteLater()-destroys the offscreen sink's
// QRhiTextureRenderTarget and QRhiRenderPassDescriptor and installs fresh ones,
// while its InvertYRenderer cached the target and renderpass by value at
// createRenderer() time. The resize fast path rebuilds the render list in place
// without reconstructing that renderer, so an upstream node's final pass would
// rebuild its graphics pipeline against a freed QRhiRenderPassDescriptor:
//   vkCreateGraphicsPipelines(): pCreateInfos[0].renderPass is not a valid
//   render pass -> VK_ERROR_VALIDATION_FAILED_EXT / SIGSEGV.
// OutputNode::currentRenderTarget() plus InvertYRenderer::init() re-adopting the
// sink's live target on every rebuild is what keeps it valid. OpenGL has no
// VkRenderPass object and is unaffected.
//
// It takes two ingredients in the same process: the incremental
// add-of-a-new-output (first case) churns the GPU allocator enough that the
// later resize (second case) rebinds a renderpass onto reused memory, which is
// why the second case is green in isolation. Whether the freed memory is still
// intact is heap-state dependent, so this is a race.
//
// Isolated in its own executable so a contained Vulkan crash cannot abort the
// rest of test_gfx_incremental.
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_incremental_findings
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_incremental_findings
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
