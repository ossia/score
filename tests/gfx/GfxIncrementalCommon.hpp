#pragma once
// =============================================================================
// Shared helpers for the L3 INCREMENTAL graph-edit tests (Priority 5).
//
// The gfx render engine can edit the render graph WHILE it renders: the fixture
// (tests/fixtures/score_test/Gfx.hpp) drives the SAME incremental paths the
// running app uses via GfxContext::incrementalEdgeUpdate —
//   * GfxPipeline::addEdgeIncremental  (addEdge + reconcile + passes + samplers)
//   * GfxPipeline::removeEdgeIncremental
//   * GfxPipeline::resizeSink          (BackgroundNode::setSize -> resize ->
//                                       Graph::recreateOutputRenderList)
// — between render() calls, not a from-scratch rebuild.
//
// These helpers collect stage snapshots outside the app lambda (Catch2 macros
// must run after run_in_gui_app teardown, per the fixture header) and assert a
// solid-colour readback.
// =============================================================================
#include "IsfTestCommon.hpp"

namespace score::test::gfx::incremental
{

/// Stage snapshots collected inside the app lambda.
struct Shot
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  ReadbackImage a, b, c;
};

/// True if every sampled pixel of a solid render equals `expect` within `tol`.
inline bool solid(const ReadbackImage& img, std::array<uint8_t, 4> expect, int tol)
{
  if(!img.valid())
    return false;
  for(int y = 2; y < img.height - 2; y += 6)
    for(int x = 2; x < img.width - 2; x += 6)
      if(!near(img.at(x, y), expect, tol))
        return false;
  return true;
}

} // namespace score::test::gfx::incremental
