// =============================================================================
// P0-6 -- RAW-RASTER MRT CORRECTNESS against a closed-form pattern.
//
// The exact shape of GfxMrtPattern.cpp, but through the raw-raster
// (RenderPipeline) MRT path instead of the ISF one. GfxRaster.cpp's
// "raw-raster MRT: two attachments" only smoke-tests that path: it asserts
// "both attachments drawn and their channel maxima differ", which a flipped,
// permuted or half-broken MRT can still satisfy. This file pins the pixels:
//
//   * every attachment k carries R = X ramp, G = Y ramp (row 0 == top),
//     B = k / (N-1), A = 1 -- fitted per pixel over a coarse interior grid;
//   * the attachments are DISTINCT and IN ORDER (centre B of attachment k
//     equals k's encoding) -- which is what catches one attachment bound N
//     times, or writes silently dropped past attachment 0.
//
// Engine surface driven (src/plugins/score-plugin-gfx/Gfx/Graph/
// RenderedRawRasterPipelineNode.cpp): a FRAGMENT_OUTPUTS count > 1 sets
// m_hasMRT ("m_hasMRT = colorCount > 1 || hasDepth || ..."), which routes
// initState through RenderedRawRasterPipelineNode::initMRTPass (the
// multi-attachment render target: "Attach ALL color textures so attachments ==
// pipeline blend targets") and lands each attachment on its output port
// through RenderedRawRasterPipelineNode::initMRTBlitPasses /
// initMRTBlitPass (one textureForOutput() blit per outgoing edge). A Y-flip in
// that blit, a wrong colorIdx in textureForOutput, or a renderTarget built
// from one texture N times are exactly the failure modes the closed form
// rejects.
//
// Motivating documents: 2026/crash-renderpipeline-mrt.score (a crash report)
// and geometric-videomapping-pipeline.score -- the only two raw-raster MRT
// user scores -- plus deferred_gbuffer / deferred_lighting in the shipped
// preset library, whose G-buffer semantics depend on each attachment being
// the right one.
//
// Geometry: syn-geo-producer.cs emits one viewport-covering triangle
// ((-1,-1),(3,-1),(-1,3)), so every pixel of every attachment is shaded and
// the ramp is asserted over the full interior, exactly like the ISF variant.
// The vertex shader derives v_uv from the pre-clipSpaceCorrMatrix position
// (GL convention, +Y up), so v_uv.y == 1 is the TOP row of the delivered
// image; the fixture's readback is Y-corrected so row 0 is the top -- the same
// orientation contract GfxMrtPattern.cpp asserts. A fullscreen triangle
// interpolates the fragment-centre form, hence the (i + 0.5) / N below.
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_raw_raster_mrt_pattern
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_raw_raster_mrt_pattern
// QT_QPA_PLATFORM=offscreen must NOT be used: it falls back to the Null
// backend, which produces a stable, self-consistent and completely wrong
// picture.
// =============================================================================
#include "IsfTestCommon.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <string>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
constexpr int kAttachments = 4;
constexpr int kTol = 6; // 8-bit quantisation of an interpolated ramp

int expected_x(int col, int width)
{
  return int(255.0 * (double(col) + 0.5) / double(width) + 0.5);
}

/// Row 0 is the top of the delivered image and the shader's v_uv.y == 1 is the
/// top, so the green ramp runs 255 at row 0 down to 0 at the last row.
int expected_y(int row, int height)
{
  return int(255.0 * (1.0 - (double(row) + 0.5) / double(height)) + 0.5);
}

int expected_id(int attachment)
{
  return int(255.0 * double(attachment) / double(kAttachments - 1) + 0.5);
}

/// Worst per-channel deviation from the closed form over a coarse grid.
struct Fit
{
  int worst = 0;
  int col = 0, row = 0;
  char channel = '?';
  int got = 0, expected = 0;

  std::string describe() const
  {
    return "channel " + std::string(1, channel) + " at (" + std::to_string(col) + ","
           + std::to_string(row) + "): got " + std::to_string(got) + ", expected "
           + std::to_string(expected);
  }
};

Fit fit_pattern(const ReadbackImage& img, int attachment)
{
  Fit f;
  const int id = expected_id(attachment);
  for(int y = 2; y < img.height - 2; y += 7)
  {
    for(int x = 2; x < img.width - 2; x += 7)
    {
      const auto px = img.at(x, y);
      const int want[4]
          = {expected_x(x, img.width), expected_y(y, img.height), id, 255};
      const char names[4] = {'R', 'G', 'B', 'A'};
      for(int c = 0; c < 4; ++c)
      {
        const int d = std::abs(int(px[c]) - want[c]);
        if(d > f.worst)
          f = Fit{d, x, y, names[c], int(px[c]), want[c]};
      }
    }
  }
  return f;
}

/// syn-geo-producer.cs (fullscreen triangle) -> raw-raster MRT node -> one
/// BackgroundNode sink per image output port; render_raster wires exactly that
/// (see tests/fixtures/score_test/Gfx.hpp: "One sink per raster image output
/// (MRT => several)").
IsfResult run_mrt(score::gfx::GraphicsApi be)
{
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_raster(
        be, {corpus("syn-geo-producer.cs")}, corpus("syn-rr-mrt-pattern.vs"),
        corpus("syn-rr-mrt-pattern.fs"));
  });
  return r;
}
} // namespace

TEST_CASE(
    "raw-raster MRT: every attachment matches its closed-form pattern",
    "[gfx][l3][raster][mrt][orientation]")
{
  const auto backend = GENERATE(from_range(platform_backends()));

  const IsfResult r = run_mrt(backend);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  if(const char* why = compute_shader_skip_reason(backend))
    SKIP(why);
  INFO("backend=" << r.backend);
  // render_raster records an empty/short sink readback as an error, so an
  // empty readback on a supported backend FAILS here instead of vacuously
  // passing.
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == std::size_t(kAttachments));

  for(int k = 0; k < kAttachments; ++k)
  {
    INFO("attachment " << k);
    REQUIRE(r.outputs[k].valid());
    const Fit f = fit_pattern(r.outputs[k], k);
    INFO(f.describe());
    CHECK(f.worst <= kTol);
  }
}

// Each attachment must be distinguishable from every other one AND sit on the
// port matching its FRAGMENT_OUTPUTS index. If initMRTPass built the render
// target from one texture N times, or textureForOutput mapped every port to
// colorIdx 0, or the blit passes all sampled the same attachment, the
// closed-form check above still passes for whichever attachment won -- this is
// what separates them.
TEST_CASE(
    "raw-raster MRT: the attachments are distinct and in order",
    "[gfx][l3][raster][mrt][orientation]")
{
  const auto backend = GENERATE(from_range(platform_backends()));

  const IsfResult r = run_mrt(backend);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  if(const char* why = compute_shader_skip_reason(backend))
    SKIP(why);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == std::size_t(kAttachments));

  // In order: the identity channel at the centre of attachment k encodes k.
  for(int k = 0; k < kAttachments; ++k)
  {
    REQUIRE(r.outputs[k].valid());
    const auto px = r.outputs[k].center();
    INFO("attachment " << k << " identity channel: got " << int(px[2])
                       << ", expected " << expected_id(k));
    CHECK(std::abs(int(px[2]) - expected_id(k)) <= kTol);
  }

  // Pairwise distinct: no two attachments carry the same centre pixel. The
  // in-order check subsumes this when it passes, but when it FAILS this one
  // tells "permuted" apart from "one attachment bound four times".
  for(int a = 0; a < kAttachments; ++a)
    for(int b = a + 1; b < kAttachments; ++b)
    {
      const auto pa = r.outputs[a].center();
      const auto pb = r.outputs[b].center();
      INFO(
          "attachments " << a << " and " << b << " centre B: " << int(pa[2]) << " vs "
                         << int(pb[2]));
      CHECK(std::abs(int(pa[2]) - int(pb[2])) > kTol);
    }
}
