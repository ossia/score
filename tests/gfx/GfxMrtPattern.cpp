// =============================================================================
// L3 MRT CORRECTNESS against a closed-form pattern.
//
// GfxOrientation.cpp pins the MRT path two ways: an analytic vertical ramp, and
// equality with the single-output render. Both are necessary and neither is
// sufficient here, because both look only at attachment 0 of a shader whose
// pattern is constant along X. That leaves three things unpinned:
//
//   * the horizontal axis  -- a ramp constant in X is unchanged by an X flip;
//   * attachments 1..N-1   -- never read, so a permutation or a stuck attachment
//                             is invisible;
//   * per-attachment identity -- two attachments carrying the same picture
//                             cannot be told apart.
//
// isf-mrt-pattern.fs closes all three: every attachment carries R = X, G = Y and
// a B that identifies the attachment. Each assertion below is 1:1 against the
// closed form, per pixel, for EVERY attachment -- not corner probes, not luma,
// not a comparison against another render that could be wrong the same way.
//
// ISF puts the origin of isf_FragNormCoord at the bottom left, so
// isf_FragNormCoord.y == 1 is the TOP row of the delivered image; the fixture's
// readback is Y-corrected so row 0 is the top. A fullscreen quad interpolates
// the fragment-centre form, hence the (i + 0.5) / N below.
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_mrt_pattern
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_mrt_pattern
// QT_QPA_PLATFORM=offscreen must NOT be used: it falls back to the Null backend,
// which produces a stable, self-consistent and completely wrong picture.
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

/// Row 0 is the top of the delivered image and ISF's y == 1 is the top, so the
/// green ramp runs 255 at row 0 down to 0 at the last row.
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
      const int want[3] = {expected_x(x, img.width), expected_y(y, img.height), id};
      const char names[3] = {'R', 'G', 'B'};
      for(int c = 0; c < 3; ++c)
      {
        const int d = std::abs(int(px[c]) - want[c]);
        if(d > f.worst)
          f = Fit{d, x, y, names[c], int(px[c]), want[c]};
      }
    }
  }
  return f;
}
} // namespace

TEST_CASE(
    "ISF MRT: every attachment matches its closed-form pattern",
    "[gfx][l3][isf][mrt][orientation]")
{
  const auto backend = GENERATE(from_range(platform_backends()));

  const IsfResult r = render(backend, {corpus("isf-mrt-pattern.fs")});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
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

// Each attachment must be distinguishable from every other one. If the engine
// bound one attachment N times, or dropped the writes to all but the first, the
// closed-form check above still passes for whichever attachment won -- this is
// what separates them.
TEST_CASE(
    "ISF MRT: the attachments are distinct and in order",
    "[gfx][l3][isf][mrt][orientation]")
{
  const auto backend = GENERATE(from_range(platform_backends()));

  const IsfResult r = render(backend, {corpus("isf-mrt-pattern.fs")});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == std::size_t(kAttachments));

  for(int k = 0; k < kAttachments; ++k)
  {
    REQUIRE(r.outputs[k].valid());
    const auto px = r.outputs[k].at(r.outputs[k].width / 2, r.outputs[k].height / 2);
    INFO("attachment " << k << " identity channel: got " << int(px[2]) << ", expected "
                       << expected_id(k));
    CHECK(std::abs(int(px[2]) - expected_id(k)) <= kTol);
  }
}
