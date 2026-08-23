// =============================================================================
// The CSF storage-image macros put compute output in the author's coordinate
// system on every backend.
//
// A compute shader that only stores -- imageStore() with a raw texel index, no
// sampled input -- is upside down on OpenGL: the index has row 0 first in
// memory everywhere, while the render target it lands in is bottom-up on GL and
// top-down elsewhere. GfxOrientationFindings.cpp documents that defect.
//
// The macros correct read and store together and are gated on the opposite side
// from the fragment macros (a fragment shader needs its correction on Vulkan, a
// compute shader on OpenGL), so both cases hold at once. This file asserts both:
//
//   * a generator written through IMG_STORE lands the right way up
//   * it agrees with the ISF reference ramp, so "consistent" cannot pass by
//     being consistently wrong
//   * every backend agrees with every other
//
// The relay case -- a CSF that samples an upstream texture and stores it at the
// matching index -- is guarded in GfxOrientation.cpp and must stay green; that
// is what rules out a store-only correction.
//
//   DISPLAY=:0 ctest -R gfx_csf_orient_macros
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

std::string row_profile(const ReadbackImage& img)
{
  std::string s;
  for(int y = 0; y < img.height; y += img.height / 8)
    s += "row " + std::to_string(y)
         + " G=" + std::to_string(int(img.at(img.width / 2, y)[1])) + "; ";
  return s;
}

/// The corpus shader writes green == gid.y / (H-1) with gid.y == 0 at the TOP,
/// so the delivered image ramps 0 -> 255 downward. A texel index, not a
/// fragment centre: the denominator is H-1.
struct RampFit
{
  int worst{}, worstRow{}, got{}, expected{};
};

RampFit fit_top_down_ramp(const ReadbackImage& img)
{
  RampFit f;
  for(int y = 0; y < img.height; ++y)
  {
    const int expected
        = int(std::lround(255.0 * double(y) / double(img.height - 1)));
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
}

TEST_CASE(
    "a CSF generator written through IMG_STORE is the right way up",
    "[gfx][l3][csf][orientation][macros]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_csf_image(backend, corpus("csf-orient-store.cs"), {}, {64, 64}, 3);
  });
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());

  const auto& img = r.outputs[0];
  INFO("profile: " << row_profile(img));
  CHECK(constant_channels_ok(img));

  const RampFit f = fit_top_down_ramp(img);
  INFO("worst row " << f.worstRow << ": green=" << f.got << " expected=" << f.expected);
  CHECK(f.worst <= kTol);
}

TEST_CASE(
    "every backend agrees on a CSF generator written through IMG_STORE",
    "[gfx][l3][csf][orientation][macros]")
{
  std::vector<std::pair<std::string, ReadbackImage>> shots;
  for(auto api : platform_backends())
  {
    IsfResult r;
    score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
      r = render_csf_image(api, corpus("csf-orient-store.cs"), {}, {64, 64}, 3);
    });
    if(r.skipped || !r.error.empty() || r.outputs.empty() || !r.outputs[0].valid())
      continue;
    shots.emplace_back(r.backend, r.outputs[0]);
  }
  if(shots.size() < 2)
    SKIP("fewer than two backends produced a picture");

  for(std::size_t i = 1; i < shots.size(); ++i)
  {
    const int d = max_channel_diff(shots[0].second, shots[i].second);
    INFO(shots[0].first << " vs " << shots[i].first << ": max channel diff " << d);
    CHECK(d <= kTol);
  }
}
