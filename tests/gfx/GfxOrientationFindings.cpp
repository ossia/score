// =============================================================================
// Two coordinate spaces meet inside a compute shader and are mirrored against
// each other on OpenGL: imageStore(img, ivec2(id)) addresses a raw texel index,
// where row 0 is first in memory on every backend, while the render target the
// result is copied into is bottom-up on OpenGL and top-down elsewhere -- what
// QRhi::isYUpInFramebuffer() reports.
//
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_orientation_findings   # green
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_orientation_findings   # RED
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
}

TEST_CASE(
    "CSF storage-image vertical orientation follows the texel-index origin",
    "[gfx][l3][csf][orientation][findings]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_csf_image(backend, corpus("csf-gradient-y.cs"), {}, {64, 64}, 3);
  });
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());

  const auto& img = r.outputs[0];
  INFO("profile: " << row_profile(img));

  // gl_GlobalInvocationID.y == 0 is the TOP row: green 0 at row 0, 255 at the
  // last row. Asserted per row against the closed-form ramp.
  int worst = 0, worstRow = 0, got = 0, expected = 0;
  for(int y = 0; y < img.height; ++y)
  {
    const int e = int(std::lround(255.0 * double(y) / double(img.height - 1)));
    for(int x : {1, img.width / 2, img.width - 2})
    {
      const int g = img.at(x, y)[1];
      if(std::abs(g - e) > worst)
      {
        worst = std::abs(g - e);
        worstRow = y;
        got = g;
        expected = e;
      }
    }
  }
  INFO("worst row " << worstRow << ": green=" << got << " expected=" << expected);
  CHECK(worst <= kTol);
}
