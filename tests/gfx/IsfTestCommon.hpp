#pragma once
// =============================================================================
// Shared helpers for the L3 ISF feature-surface tests (tests/gfx/Isf*.cpp).
//
// These are TEST-only conveniences layered on top of the render+readback
// fixture in tests/fixtures/score_test/Gfx.hpp (which this does NOT modify).
// They factor out the two things every ISF test file needs:
//
//   * corpus(name)          — absolute path to a committed corpus shader.
//   * render(backend,chain) — render a linear ISF chain on ONE backend inside a
//                             booted GUI app and return the readback result
//                             (Catch2 macros must run *after*, per the fixture
//                             header — so this only collects, never asserts).
//   * render_all(chain)     — render the SAME chain on EVERY platform backend
//                             and return one result per backend, so a test can
//                             assert the readbacks AGREE across backends (the
//                             GL-vs-Vulkan divergence check the plan calls for).
//
// Orientation: the final sink readback is Y-corrected (row 0 == top) and both
// axes agree across every backend, at any chain depth. Assert the vertical axis
// as freely as the horizontal one -- GfxOrientationMatrix.cpp pins exactly that,
// on OpenGL, Vulkan, llvmpipe, lavapipe, Direct3D 11, Direct3D 12 and Metal.
//
//   This header used to say the vertical axis of a cross-node sampled image
//   could differ between backends, and told tests to key on the X gradient or
//   the centre instead. That was a real defect, not a property: an ISF node with
//   more than one declared OUTPUT reaches its destination through a blit, and
//   that blit flipped the picture on HLSL and MSL. Fixed by removing the flip;
//   the workaround is no longer needed and no new test should adopt it.
// =============================================================================

#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <string>
#include <vector>

namespace score::test::gfx::isf
{

inline QString corpus(const char* file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}

/// Render a linear ISF chain on ONE backend; collect the readback only.
inline IsfResult
render(score::gfx::GraphicsApi backend, std::vector<QString> chain,
       QSize size = {64, 64}, int frames = 3)
{
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = score::test::gfx::render_isf_chain(backend, std::move(chain), size, frames);
  });
  return r;
}

/// One backend's shot in a cross-backend agreement comparison.
struct BackendShot
{
  score::gfx::GraphicsApi api;
  IsfResult result;
};

/// Render the same chain on EVERY platform backend (each in its own booted app)
/// and return one shot per backend. The caller decides which are skipped and
/// compares the non-skipped ones for agreement.
inline std::vector<BackendShot>
render_all(std::vector<QString> chain, QSize size = {64, 64}, int frames = 3)
{
  std::vector<BackendShot> shots;
  for(auto api : score::test::gfx::platform_backends())
    shots.push_back({api, render(api, chain, size, frames)});
  return shots;
}

/// Largest per-channel absolute difference between two readbacks over a coarse
/// grid (skips the 1px border, where rasterizer rounding differs most).
inline int max_channel_diff(const ReadbackImage& a, const ReadbackImage& b)
{
  if(a.width != b.width || a.height != b.height)
    return 256;
  int worst = 0;
  for(int y = 2; y < a.height - 2; y += 3)
    for(int x = 2; x < a.width - 2; x += 3)
    {
      const auto pa = a.at(x, y);
      const auto pb = b.at(x, y);
      for(int c = 0; c < 4; ++c)
        worst = std::max(worst, std::abs(int(pa[c]) - int(pb[c])));
    }
  return worst;
}

/// True if the image is not a single constant colour (i.e. it actually rendered
/// structure, not a cleared / degenerate buffer).
inline bool non_degenerate(const ReadbackImage& img)
{
  const auto first = img.at(2, 2);
  for(int y = 2; y < img.height - 2; y += 2)
    for(int x = 2; x < img.width - 2; x += 2)
      if(!near(img.at(x, y), first, 6))
        return true;
  return false;
}

} // namespace score::test::gfx::isf
