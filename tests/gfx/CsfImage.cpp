// =============================================================================
// L3 GPU render + readback tests — CSF (compute shader) IMAGE outputs, across
// EVERY RHI backend available on this machine.
//
// These drive real COMPUTE_SHADER (.cs) nodes through the offscreen fixture:
// each CSF's write image RESOURCE is written by a compute dispatch
// (RenderedCSFNode), blitted to the sink and read back as RGBA8. Every test
// iterates score::test::gfx::platform_backends() with Catch2 GENERATE, so the
// identical compute readback is validated on OpenGL, Vulkan, ... — the point of
// L3 is to catch backend-specific compute-path regressions.
//
// A backend that genuinely cannot initialize here is SKIPped for that backend
// only. All GPU work runs on the main thread inside run_in_gui_app; pixels are
// harvested there and assertions run afterwards.
//
// Run:  DISPLAY=:0 ctest -R gfx_csf --output-on-failure
//       SCORE_TEST_API=opengl|vulkan to restrict to one backend.
// =============================================================================

#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

namespace
{
QString corpus(const char* file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}

using score::test::gfx::IsfResult;
using score::test::gfx::near;
using score::test::gfx::ReadbackImage;

// Render a (possibly mixed ISF/CSF) chain on `backend` inside a booted GUI app
// and return the pixels. Catch2 macros run *after* this returns.
IsfResult render(score::gfx::GraphicsApi backend, std::vector<QString> chain)
{
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = score::test::gfx::render_isf_chain(backend, std::move(chain));
  });
  return r;
}

// True if the image is not a single constant colour over a coarse interior grid.
bool non_degenerate(const ReadbackImage& img)
{
  const auto first = img.at(2, 2);
  for(int y = 2; y < img.height - 2; y += 2)
    for(int x = 2; x < img.width - 2; x += 2)
      if(!near(img.at(x, y), first, 6))
        return true;
  return false;
}
}

// -----------------------------------------------------------------------------
// csf-image-r32f.cs: 2D_IMAGE dispatch writes a single-channel R32F image with a
// radial falloff  val = (1 - smoothstep(0,1,dist)) * (0.5 + 0.5*sin(TIME*2)).
// The fixture drives TIME ~= 0 (date == frame index), so the time factor is
// ~0.5 and the result is a stable grayscale radial gradient: bright at the
// centre (dist 0 -> val ~0.5 -> ~128), dark at the corners (dist -> 1 -> val 0).
// The single-channel blit reads back as vec4(v,v,v,1) (grayscale).
// -----------------------------------------------------------------------------
TEST_CASE("csf-image-r32f writes a radial R32F gradient", "[gfx][l3][csf]")
{
  const auto backend = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(backend));

  const IsfResult r = render(backend, {corpus("csf-image-r32f.cs")});

  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);

  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);

  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  const auto c = img.center();
  const auto corner = img.at(1, 1);
  INFO("centre=(" << (int)c[0] << "," << (int)c[1] << "," << (int)c[2] << ","
                  << (int)c[3] << ") corner=(" << (int)corner[0] << ","
                  << (int)corner[1] << "," << (int)corner[2] << ")");

  // Grayscale: R == G == B at the centre.
  CHECK(std::abs(int(c[0]) - int(c[1])) <= 4);
  CHECK(std::abs(int(c[0]) - int(c[2])) <= 4);
  CHECK(c[3] >= 250); // opaque

  // Radial falloff: centre distinctly brighter than the corner.
  CHECK(int(c[0]) > int(corner[0]) + 40);
  // Centre near the analytic value (radial 1.0 * 0.5 -> 128), generous tol.
  CHECK(std::abs(int(c[0]) - 128) <= 40);
  CHECK(corner[0] <= 24); // corner ~ black
}

// -----------------------------------------------------------------------------
// csf-image-rgba16f.cs: RGBA16F image with expression dimensions ($WIDTH/$HEIGHT)
// writes HDR values  r = uv.x*2, g = uv.y*2, b = sin(TIME + uv.x*10)*0.5+0.5.
// Blitted through RGBA8 (values clamp to [0,1]). The horizontal (red) axis is the
// backend-stable one: red ramps left->right and saturates for uv.x >= 0.5.
// -----------------------------------------------------------------------------
TEST_CASE("csf-image-rgba16f writes an HDR RGBA16F ramp", "[gfx][l3][csf]")
{
  const auto backend = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(backend));

  const IsfResult r = render(backend, {corpus("csf-image-rgba16f.cs")});

  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);

  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);

  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  const int w = img.width, h = img.height;
  const auto left = img.at(w / 8, h / 2);
  const auto right = img.at(7 * w / 8, h / 2);
  const auto c = img.center();
  INFO("left.R=" << (int)left[0] << " right.R=" << (int)right[0]
                 << " centre=(" << (int)c[0] << "," << (int)c[1] << ","
                 << (int)c[2] << ")");

  // Red ramps up along +X (uv.x * 2, clamped): left dark-red, right saturated.
  CHECK(int(right[0]) > int(left[0]) + 40);
  CHECK(left[0] <= 120);
  CHECK(right[0] >= 230);
  // Centre uv ~ (0.5, 0.5): r = 1.0 -> 255, g = 1.0 -> 255.
  CHECK(c[0] >= 200);
  CHECK(c[1] >= 200);
  CHECK(c[3] >= 250);
  CHECK(non_degenerate(img));
}

// -----------------------------------------------------------------------------
// csf-texture-sampling.cs: samples an upstream texture (sampler2D) in the compute
// shader and writes the INVERTED colour to its output image. Feeding the known
// solid magenta (1,0,1) input, the compute must read it and produce
// (1-1, 1-0, 1-1) = (0,1,0) = green. Proves texture() works in a CSF dispatch.
// -----------------------------------------------------------------------------
TEST_CASE("csf-texture-sampling inverts a sampled input texture", "[gfx][l3][csf]")
{
  const auto backend = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(backend));

  const IsfResult r = render(
      backend, {corpus("isf-solid-color.fs"), corpus("csf-texture-sampling.cs")});

  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);

  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);

  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  const auto c = img.center();
  INFO("centre=(" << (int)c[0] << "," << (int)c[1] << "," << (int)c[2] << ","
                  << (int)c[3] << ")");

  // Inverted magenta -> green.
  const std::array<uint8_t, 4> green{0, 255, 0, 255};
  CHECK(near(c, green, 6));
  CHECK(near(img.at(img.width / 4, img.height / 4), green, 6));
  CHECK(near(img.at(3 * img.width / 4, 3 * img.height / 4), green, 6));
}
