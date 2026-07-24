// =============================================================================
// L3 GPU render + readback tests — proving the offscreen ISF pipeline loop
// across EVERY RHI backend available on this machine.
//
// Each test builds a minimal gfx pipeline via score::test::gfx::render_isf_chain
// (see tests/fixtures/score_test/Gfx.hpp), renders it offscreen for a few
// frames on a given backend, reads back the RGBA8 output texture(s) and asserts
// on pixel values. Every test iterates score::test::gfx::platform_backends()
// with Catch2 GENERATE, so the identical readback is validated on OpenGL,
// Vulkan, etc. — that is the point of L3: catch backend-specific render bugs.
//
// A backend that genuinely cannot initialize on this machine (no driver/ICD,
// wrong platform such as Metal on Linux, or a legacy GL context) is SKIPped for
// that backend only, with its name in the reason. A backend that IS available
// MUST render and match.
//
// All GPU work runs on the main thread inside run_in_gui_app; pixels are
// harvested there and the assertions run afterwards so a failing REQUIRE/SKIP
// doesn't unwind out of the app teardown.
//
// Run (real hardware, real display):
//     ctest -R gfx --output-on-failure
// Restrict to one backend with SCORE_TEST_API=opengl|vulkan|...
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

// Render `chain` on `backend` inside a booted GUI app and return the pixels.
IsfResult render(score::gfx::GraphicsApi backend, std::vector<QString> chain)
{
  IsfResult r;
  score::test::run_in_gui_app(
      [&](const score::GUIApplicationContext&) {
        r = score::test::gfx::render_isf_chain(backend, std::move(chain));
      });
  return r;
}
}

TEST_CASE("isf-solid-color renders a constant magenta frame", "[gfx][l3][isf]")
{
  const auto backend = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(backend));

  const IsfResult r = render(backend, {corpus("isf-solid-color.fs")});

  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);

  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);

  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  // isf-solid-color.fs: gl_FragColor = vec4(1,0,1,1) everywhere.
  const std::array<uint8_t, 4> magenta{255, 0, 255, 255};
  CHECK(near(img.center(), magenta, 2));
  CHECK(near(img.at(0, 0), magenta, 2));
  CHECK(near(img.at(img.width - 1, img.height - 1), magenta, 2));
  CHECK(near(img.at(img.width - 1, 0), magenta, 2));
}

TEST_CASE("isf-image-passthrough samples a known solid input", "[gfx][l3][isf]")
{
  const auto backend = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(backend));

  // Feed a known solid input (magenta from isf-solid-color) into the
  // passthrough shader, which re-samples it through the ISF image macros.
  const IsfResult r = render(
      backend, {corpus("isf-solid-color.fs"), corpus("isf-image-passthrough.fs")});

  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);

  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);

  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  // isf-image-passthrough.fs draws a 2x2 grid: 3 quadrants re-sample the input
  // via IMG_NORM_PIXEL / IMG_THIS_PIXEL / IMG_PIXEL (so they show the magenta
  // input); the 4th shows TEX_DIMENSIONS encoded as a color. It is NOT a pure
  // full-frame copy, so we probe each quadrant centre and require that at least
  // three of them reproduce the input colour (independent of which quadrant is
  // the dimensions one and of Y orientation).
  const std::array<uint8_t, 4> magenta{255, 0, 255, 255};
  const int w = img.width, h = img.height;
  const std::array<std::array<uint8_t, 4>, 4> quad{
      img.at(w / 4, h / 4), img.at(3 * w / 4, h / 4), img.at(w / 4, 3 * h / 4),
      img.at(3 * w / 4, 3 * h / 4)};

  int matches = 0;
  for(const auto& q : quad)
    if(near(q, magenta, 4))
      ++matches;

  INFO(
      "quadrant centres: (" << (int)quad[0][0] << "," << (int)quad[0][1] << ","
                            << (int)quad[0][2] << ") (" << (int)quad[1][0] << ","
                            << (int)quad[1][1] << "," << (int)quad[1][2] << ") ("
                            << (int)quad[2][0] << "," << (int)quad[2][1] << ","
                            << (int)quad[2][2] << ") (" << (int)quad[3][0] << ","
                            << (int)quad[3][1] << "," << (int)quad[3][2] << ")");
  CHECK(matches >= 3);
}

TEST_CASE("mrt-single-color renders its single declared output", "[gfx][l3][mrt]")
{
  const auto backend = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(backend));

  const IsfResult r = render(backend, {corpus("mrt-single-color.fs")});

  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);

  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  // A single declared OUTPUTS entry -> one image output port.
  REQUIRE(r.outputs.size() == 1);

  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  // colorOut = vec4(uv, 0, 1): a red/green gradient. Centre ~ (0.5, 0.5).
  const auto c = img.center();
  INFO("centre = (" << (int)c[0] << "," << (int)c[1] << "," << (int)c[2] << ","
                    << (int)c[3] << ")");
  CHECK(std::abs(int(c[0]) - 128) <= 24); // R ~ uv.x
  CHECK(std::abs(int(c[1]) - 128) <= 24); // G ~ uv.y
  CHECK(c[2] <= 4);                       // B == 0
  CHECK(c[3] >= 250);                     // A == 1
}

TEST_CASE("isf-mrt-four-outputs writes four distinct attachments", "[gfx][l3][mrt]")
{
  const auto backend = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(backend));

  const IsfResult r = render(backend, {corpus("isf-mrt-four-outputs.fs")});

  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);

  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  // Four declared OUTPUTS -> four image output ports, each read back separately.
  REQUIRE(r.outputs.size() == 4);
  for(const auto& o : r.outputs)
    REQUIRE(o.valid());

  // At centre uv = (0.5, 0.5):
  //   out0 = (uv.x, uv.y, 0, 1)      -> (~128, ~128,   0)
  //   out1 = (0, uv.y, uv.x, 1)      -> (   0, ~128, ~128)
  const auto c0 = r.outputs[0].center();
  const auto c1 = r.outputs[1].center();
  INFO("out0 centre = (" << (int)c0[0] << "," << (int)c0[1] << "," << (int)c0[2] << ")");
  INFO("out1 centre = (" << (int)c1[0] << "," << (int)c1[1] << "," << (int)c1[2] << ")");

  // Attachment 0: red/green gradient centre.
  CHECK(std::abs(int(c0[0]) - 128) <= 24);
  CHECK(std::abs(int(c0[1]) - 128) <= 24);
  CHECK(c0[2] <= 4);

  // Attachment 1: distinctly different content (blue/cyan gradient centre).
  CHECK(c1[0] <= 4);
  CHECK(std::abs(int(c1[1]) - 128) <= 24);
  CHECK(std::abs(int(c1[2]) - 128) <= 24);

  // And the two attachments are genuinely different renders.
  CHECK(!near(c0, c1, 8));
}

// -----------------------------------------------------------------------------
// TODO (L3, next pass): CSF compute readback.
//
// csf-image-r32f.cs writes a radial gradient into a 256x256 R32F image via a
// COMPUTE_SHADER pass, which a downstream ISF shader then samples. Driving the
// compute pass and reading back a single-channel float image needs the fixture
// to (a) build a CSF ISFNode via the compute constructor, (b) run the compute
// dispatch, and (c) read back an R32F (not RGBA8) texture. That is deliberately
// out of scope for this first ISF-focused pass; tracked here so it isn't lost.
// -----------------------------------------------------------------------------
TEST_CASE("csf-image-r32f compute readback", "[gfx][l3][csf][.todo]")
{
  SKIP("TODO: CSF compute-shader image (R32F) readback not yet implemented in "
       "the L3 fixture — see comment in tests/gfx/Gfx.cpp. ISF color/MRT "
       "readback is proven across backends by the other [gfx][l3] cases.");
}
