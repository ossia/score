// =============================================================================
// L3 VERTICAL ORIENTATION of the render engine's texture outputs.
//
// Every path producing a texture in score::gfx must deliver the same picture,
// right way up, on every RHI backend. ISF fragment shaders put the origin of
// isf_FragNormCoord at the bottom-left per the ISF specification, so
// isf_FragNormCoord.y == 1 is the TOP row of the delivered image.
//
// The single-output ISF path (isf-gradient-y.fs) is the reference. The MRT path
// must agree with it, and is additionally asserted against the shader's own
// analytic ramp so "they agree" cannot pass by both being wrong the same way.
//
// The CSF compute storage-image origin is a separate, still-open defect in its
// own isolated target -- see GfxOrientationFindings.cpp.
//
// Assertions are per-row and 1:1 against the closed-form ramp, not corner probes,
// luma or "non-black", because a vertical flip passes every one of those.
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_orientation
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_orientation
// QT_QPA_PLATFORM=offscreen must NOT be used: it falls back to the Null backend,
// which produces a stable, self-consistent and completely wrong picture.
// =============================================================================
#include "IsfTestCommon.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
constexpr int kTol = 8;

struct RampFit
{
  int worst = 0;    // largest |got - expected| over the sampled rows
  int worstRow = 0; // row where that happened
  int got = 0;
  int expected = 0;
};

/// Compare the GREEN channel of `img`, column by column ignored (the ramp is
/// constant along X), against a linear ramp over the rows.
///   topIsZero == false : green == 255 at row 0, 0 at the last row (ISF)
///   topIsZero == true  : green == 0 at row 0, 255 at the last row (compute)
/// `denomIsRowCount` selects the fragment-centre form ((y+0.5)/H, what a
/// rasterized fullscreen quad interpolates) over the texel-index form
/// (y/(H-1), what the compute shader writes).
RampFit
fit_vertical_ramp(const ReadbackImage& img, bool topIsZero, bool fragmentCentre)
{
  RampFit f;
  const int H = img.height;
  for(int y = 0; y < H; ++y)
  {
    const double t = fragmentCentre ? (double(y) + 0.5) / double(H)
                                    : double(y) / double(H - 1);
    const double v = topIsZero ? t : 1.0 - t;
    const int expected = int(std::lround(255.0 * v));

    // Sample a few columns; the ramp does not vary along X.
    for(int x : {1, img.width / 2, img.width - 2})
    {
      const int got = img.at(x, y)[1];
      const int d = std::abs(got - expected);
      if(d > f.worst)
        f = {d, y, got, expected};
    }
  }
  return f;
}

/// The constant red (0.25) / blue (0.75) channels every orientation corpus
/// shader writes: a guard against reading back a cleared or garbage target.
bool constant_channels_ok(const ReadbackImage& img)
{
  for(int y = 1; y < img.height - 1; y += 7)
    for(int x = 1; x < img.width - 1; x += 7)
    {
      const auto p = img.at(x, y);
      if(std::abs(int(p[0]) - 64) > kTol || std::abs(int(p[2]) - 191) > kTol)
        return false;
    }
  return true;
}

std::string row_profile(const ReadbackImage& img)
{
  std::string s;
  for(int y = 0; y < img.height; y += img.height / 8)
    s += "row " + std::to_string(y) + " G=" + std::to_string(int(img.at(img.width / 2, y)[1]))
         + "; ";
  return s;
}
}

// -----------------------------------------------------------------------------
// REFERENCE: single-output ISF. isf_FragNormCoord.y == 1 at the top.
// -----------------------------------------------------------------------------
TEST_CASE(
    "ISF single-output vertical orientation follows the ISF bottom-left origin",
    "[gfx][l3][isf][orientation]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const IsfResult r = render(backend, {corpus("isf-gradient-y.fs")});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());

  const auto& img = r.outputs[0];
  INFO("profile: " << row_profile(img));
  CHECK(constant_channels_ok(img));

  const RampFit f = fit_vertical_ramp(img, /*topIsZero=*/false, /*fragmentCentre=*/true);
  INFO(
      "worst row " << f.worstRow << ": green=" << f.got << " expected=" << f.expected);
  CHECK(f.worst <= kTol);
}

// -----------------------------------------------------------------------------
// MRT (>1 declared OUTPUTS) must deliver the same orientation as the reference.
// -----------------------------------------------------------------------------
TEST_CASE(
    "ISF MRT vertical orientation matches the single-output path",
    "[gfx][l3][isf][mrt][orientation]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const IsfResult r = render(backend, {corpus("isf-mrt-gradient-y.fs")});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 2);
  REQUIRE(r.outputs[0].valid());

  const auto& img = r.outputs[0];
  INFO("profile: " << row_profile(img));
  CHECK(constant_channels_ok(img));

  const RampFit f = fit_vertical_ramp(img, /*topIsZero=*/false, /*fragmentCentre=*/true);
  INFO(
      "worst row " << f.worstRow << ": green=" << f.got << " expected=" << f.expected);
  CHECK(f.worst <= kTol);
}

// -----------------------------------------------------------------------------
// The MRT attachment and the single-output render of the same expression must
// be the same image. Asserted directly, so a fix that flips BOTH would still be
// caught by the analytic checks above.
// -----------------------------------------------------------------------------
TEST_CASE(
    "ISF MRT attachment is pixel-identical to the single-output render",
    "[gfx][l3][isf][mrt][orientation]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const IsfResult ref = render(backend, {corpus("isf-gradient-y.fs")});
  const IsfResult mrt = render(backend, {corpus("isf-mrt-gradient-y.fs")});
  if(ref.skipped || mrt.skipped)
    SKIP(ref.backend + ": backend unavailable");
  INFO("backend=" << ref.backend);
  REQUIRE(ref.error.empty());
  REQUIRE(mrt.error.empty());
  REQUIRE(ref.outputs.size() == 1);
  REQUIRE(mrt.outputs.size() == 2);

  INFO("single-output profile: " << row_profile(ref.outputs[0]));
  INFO("MRT attachment profile: " << row_profile(mrt.outputs[0]));
  CHECK(max_channel_diff(ref.outputs[0], mrt.outputs[0]) <= kTol);
}

// -----------------------------------------------------------------------------
// A compute shader that SAMPLES an upstream texture and stores it must relay the
// picture unturned. This is the half of the CSF orientation question that is
// currently CORRECT on every backend, and it is pinned here precisely because
// the obvious fix for the generator case (flipping the compute node's output
// blit on OpenGL) silently breaks it -- see GfxOrientationFindings.cpp.
// csf-texture-sampling.cs inverts the colour, so green comes back as 1 - the
// producer's ramp.
// -----------------------------------------------------------------------------
TEST_CASE(
    "CSF relaying a sampled texture preserves vertical orientation",
    "[gfx][l3][csf][orientation]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const IsfResult r
      = render(backend, {corpus("isf-gradient-y.fs"), corpus("csf-texture-sampling.cs")});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());

  const auto& img = r.outputs[0];
  INFO("profile: " << row_profile(img));

  // Inverted producer ramp: green == 0 at the top, 255 at the bottom.
  const RampFit f = fit_vertical_ramp(img, /*topIsZero=*/true, /*fragmentCentre=*/true);
  INFO(
      "worst row " << f.worstRow << ": green=" << f.got << " expected=" << f.expected);
  CHECK(f.worst <= kTol);
}
