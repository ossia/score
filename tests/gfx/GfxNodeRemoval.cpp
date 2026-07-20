// =============================================================================
// L3 INCREMENTAL — node removal must release transitively-unreachable upstream
// renderers (Priority 5 / finding R2-#11, leak + latent UAF).
//
// Chain A -> M -> N -> sink. Removing the middle node N makes its whole upstream
// chain (M and A) unreachable from the output. removeNodeAndEdges cleaned up only
// N's own renderers then called the bare retopologicalSort() per render list,
// which rebuilds rl->nodes/renderers from the reachable set but NEVER releases
// the orphaned upstream renderers nor erases their node->renderedNodes[rl]
// entries — a GPU-resource leak, and a later use-after-free when the RenderList
// is destroyed and those stale renderers call releaseState on the freed list.
//
// Fix (commit 48a9a1a7b): removeNodeAndEdges calls reconcileAllRenderLists(),
// whose step 3 deletes+erases exactly those unreachable renderers.
//
// REGRESSION GUARD (structural, backend-independent but run per backend so the
// real GPU renderers exist). After removing N: M and A must have NO leftover
// renderedNodes entry (post-fix: erased; pre-fix: a stale, dangling entry).
// Removing M afterwards, and tearing the pipeline down, must not crash. GREEN on
// OpenGL and Vulkan. Do NOT weaken.
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_node_removal
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_node_removal
// =============================================================================
#include "GfxIncrementalCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;
using namespace score::test::gfx::incremental;

TEST_CASE(
    "removeNode releases transitively-unreachable upstream renderers",
    "[gfx][l3][incremental][removal]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Shot out;
  // Captured inside the app lambda (the RenderList / renderer pointers are freed
  // at teardown, so the structural checks must run there).
  bool ran = false;
  std::size_t mRenderersBefore = 0, aRenderersBefore = 0;
  std::size_t mRenderersAfterRemoveN = 0, aRenderersAfterRemoveN = 0;
  bool survivedFurtherRemoval = false;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int a = p.addIsf(corpus("isf-solid-color.fs"));       // magenta source
    const int m = p.addIsf(corpus("isf-passthrough-plain.fs")); // upstream mid
    const int n = p.addIsf(corpus("isf-passthrough-plain.fs")); // to be removed
    const int s0 = p.addSink({64, 64});
    p.wire(p.imageOut(a, 0), p.imageIn(m, 0)); // A -> M
    p.wire(p.imageOut(m, 0), p.imageIn(n, 0)); // M -> N
    p.wire(p.imageOut(n, 0), p.sinkInput(s0)); // N -> sink

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
    out.a = p.readback(s0); // sink shows A's magenta through M, N

    mRenderersBefore = p.isf(m)->renderedNodes.size();
    aRenderersBefore = p.isf(a)->renderedNodes.size();

    // Remove the middle node N: M and A become unreachable from the sink.
    p.removeNodeIncremental(n);

    mRenderersAfterRemoveN = p.isf(m)->renderedNodes.size();
    aRenderersAfterRemoveN = p.isf(a)->renderedNodes.size();

    // The sink is now idle (its only feeding edge was N->sink). Keep rendering
    // and remove the now-orphaned M too: the pre-fix stale renderedNodes[rl]
    // entry is what a later releaseState-on-freed-list UAF trips over.
    p.render(3);
    out.b = p.readback(s0);
    p.removeNodeIncremental(m);
    p.render(3);
    out.c = p.readback(s0);
    survivedFurtherRemoval = true;
    ran = true;
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty());
  REQUIRE(ran);

  // Before removal, the chain rendered magenta into the sink.
  REQUIRE(out.a.valid());
  CHECK(solid(out.a, {255, 0, 255, 255}, 3));

  // Both upstream nodes had live renderers before the removal.
  INFO("M renderers before=" << mRenderersBefore << " after=" << mRenderersAfterRemoveN);
  INFO("A renderers before=" << aRenderersBefore << " after=" << aRenderersAfterRemoveN);
  CHECK(mRenderersBefore >= 1);
  CHECK(aRenderersBefore >= 1);

  // THE GUARD: after removing N, the transitively-unreachable M and A must have
  // their renderer released and their renderedNodes entry erased. Pre-fix these
  // stay non-empty (leaked + dangling), which is the exact defect.
  CHECK(mRenderersAfterRemoveN == 0);
  CHECK(aRenderersAfterRemoveN == 0);

  // No crash across the subsequent render + removal + teardown.
  CHECK(survivedFurtherRemoval);
}
