// =============================================================================
// The CSF compute storage-image ORIGIN, and the macro that resolves it.
//
// A compute shader that only stores -- imageStore() with a raw texel index, no
// sampled input -- used to come out vertically flipped on OpenGL and correct on
// Vulkan. Two coordinate spaces meet inside a compute shader and are mirrored
// against each other on OpenGL: imageStore(img, ivec2(id)) addresses a raw texel
// index, where row 0 is first in memory on every backend, while the render
// target the result is copied into is bottom-up on OpenGL and top-down elsewhere
// -- what QRhi::isYUpInFramebuffer() reports.
//
// Two one-line fixes were measured and both rejected, and they are why the
// accepted fix has the shape it does:
//   1. Flipping the compute node's output blit on OpenGL turned the generator
//      case green and the RELAY case red. A CSF that samples an upstream texture
//      and stores it at the matching index was correct precisely because the
//      read and the store were mirrored the same way and cancelled.
//   2. Appending GLSL45.defaultFunctions in parse_csf() gave a CSF the fragment
//      macros, whose ISF_FIXUP_TEXCOORD is gated on QSHADER_SPIRV -- so it would
//      have flipped Vulkan, the backend that was already correct.
//
// Resolved instead by GLSL45.computeImageMacros: IMG_STORE / IMG_LOAD and the
// sampling macros correct the store AND the read together, on OpenGL alone, so a
// generator lands in the author's top-down model while a relay stays consistent
// with itself. This file now guards that csf-gradient-y.cs, written through
// IMG_STORE, is the right way up on every backend; the relay guard lives in
// GfxOrientation.cpp and the cross-backend agreement in GfxCsfOrientMacros.cpp.
//
// A CSF that still calls imageStore() directly is still upside down on OpenGL:
// the macro is the contract, not an automatic rewrite.
//
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_orientation_findings
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_orientation_findings
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
