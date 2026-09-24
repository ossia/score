// =============================================================================
// EVERY EXAMPLE IN THE SHADER GUIDE ACTUALLY RENDERS.
//
// The guide's value is that an agent can paste an example and have it work, so
// each one is pinned here by the picture it produces, not merely by the fact
// that it compiles. ShaderCorpusTargets.cpp already bakes every corpus file for
// SPIR-V, GLSL, HLSL and MSL; this file is the other half -- it runs them and
// reads pixels back.
//
// The examples live in tests/gfx/corpus as guide-*.{fs,vs,cs} so the bake sweep
// picks them up for free.
//
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_shader_guide_examples
// =============================================================================
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

// Every render helper probes the RHI, which needs a live QGuiApplication --
// calling one straight from a TEST_CASE body segfaults inside
// QVulkanInstance::supportedApiVersion().
IsfResult run_isf(score::gfx::GraphicsApi be, const char* f, QSize sz)
{
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_isf_chain(be, {corpus(f)}, sz);
  });
  return r;
}

IsfResult raster(
    score::gfx::GraphicsApi be, const char* cs, const char* vs, const char* fs,
    QSize sz, int frames = 3)
{
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_raster(be, {corpus(cs)}, corpus(vs), corpus(fs), sz, frames);
  });
  return r;
}
}

// Example 1. A plain ISF fragment shader: red ramps left-to-right, green is a
// constant 0.5. Both halves matter -- a constant green that survives proves the
// shader ran, and a red that changes across x proves isf_FragNormCoord is live.
TEST_CASE("guide: a minimal ISF shader ramps across x", "[gfx][l3][guide][isf]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  const auto r = run_isf(be, "guide-isf-basic.fs", {128, 64});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error='" << r.error << "'");
  REQUIRE(r.error.empty());
  REQUIRE(!r.outputs.empty());
  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  const auto left = img.at(4, 32);
  const auto right = img.at(img.width - 5, 32);
  INFO("left rgb " << (int)left[0] << "," << (int)left[1] << "," << (int)left[2]
                   << "  right rgb " << (int)right[0] << "," << (int)right[1]
                   << "," << (int)right[2]);
  CHECK(left[0] < 32);           // ramp starts near 0
  CHECK(right[0] > 200);         // and reaches near 1
  CHECK(right[0] > left[0]);
  CHECK(left[1] > 100);          // constant green: the shader ran
  CHECK(left[1] < 160);
}

// Examples 5 + 2 together: a compute shader produces a 3-vertex triangle, a
// raw-raster pass on the geometry path draws it. This is the pairing the guide
// presents as the canonical generative chain.
TEST_CASE(
    "guide: a CSF triangle draws through the geometry path",
    "[gfx][l3][guide][rawraster][csf]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  const auto r = raster(
      be, "guide-csf-geometry.cs", "guide-rawraster-geo.vs",
      "guide-rawraster-geo.fs", {128, 128});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error='" << r.error << "'");
  REQUIRE(r.error.empty());
  REQUIRE(!r.outputs.empty());
  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  // The triangle spans x [-0.6, 0.6], y [-0.5, 0.6] in NDC, so its centroid is
  // comfortably inside the frame and the corners are not.
  const auto centre = img.at(img.width / 2, img.height / 2);
  const auto corner = img.at(2, 2);
  INFO("centre rgb " << (int)centre[0] << "," << (int)centre[1] << ","
                     << (int)centre[2] << "  corner rgb " << (int)corner[0]
                     << "," << (int)corner[1] << "," << (int)corner[2]);

  // Something was drawn at the centre...
  const int centreSum = centre[0] + centre[1] + centre[2];
  CHECK(centreSum > 60);
  // ...and the vertex colours interpolated rather than collapsing to one hue.
  CHECK(centre[0] > 10);
  CHECK(centre[1] > 10);
  // ...while the corner, outside the triangle, stayed background.
  CHECK(corner[0] + corner[1] + corner[2] < centreSum);
}

// Example 4. A compute shader writing a storage image, read back through the
// image output it declares. Also pins the top-down texel convention the guide
// calls out: the ramp is on x here, so both ends are sampled on one row.
TEST_CASE("guide: a CSF writes a storage image", "[gfx][l3][guide][csf]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));
  if(const char* why = compute_shader_skip_reason(be))
    SKIP(why);

  const auto r = run_isf(be, "guide-csf-image.cs", {64, 64});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error='" << r.error << "'");
  REQUIRE(r.error.empty());
  REQUIRE(!r.outputs.empty());
  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  const auto left = img.at(2, 32);
  const auto right = img.at(img.width - 3, 32);
  INFO("left rgb " << (int)left[0] << "," << (int)left[1] << "," << (int)left[2]
                   << "  right rgb " << (int)right[0] << "," << (int)right[1]
                   << "," << (int)right[2]);
  // red ramps up, blue ramps down, green is the constant 0.25 witness.
  CHECK(right[0] > left[0]);
  CHECK(right[2] < left[2]);
  CHECK(left[1] > 40);
  CHECK(left[1] < 90);
}

// Example 6. Frame-to-frame feedback. The oracle is the whole point: the same
// chain rendered for longer must come back brighter, which distinguishes a
// buffer that genuinely persists from one cleared or reallocated every frame.
// A single-frame reading could not tell those apart.
TEST_CASE(
    "guide: a read_write attribute accumulates across frames",
    "[gfx][l3][guide][csf][feedback]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  const auto few = raster(
      be, "guide-csf-feedback.cs", "guide-rawraster-geo.vs",
      "guide-rawraster-geo.fs", {96, 96}, 3);
  const auto many = raster(
      be, "guide-csf-feedback.cs", "guide-rawraster-geo.vs",
      "guide-rawraster-geo.fs", {96, 96}, 20);
  if(few.skipped || many.skipped)
    SKIP(few.backend + ": " + few.skip_reason);

  INFO("few error='" << few.error << "' many error='" << many.error << "'");
  REQUIRE(few.error.empty());
  REQUIRE(many.error.empty());
  REQUIRE(!few.outputs.empty());
  REQUIRE(!many.outputs.empty());
  REQUIRE(few.outputs[0].valid());
  REQUIRE(many.outputs[0].valid());

  const auto a = few.outputs[0].at(48, 48);
  const auto b = many.outputs[0].at(48, 48);
  INFO("centre red after 3 frames = " << (int)a[0] << ", after 20 = " << (int)b[0]);
  CHECK(a[0] > 0);       // it drew at all
  CHECK(b[0] > a[0]);    // and the accumulator survived between frames
}
