// Compute shader fixes from the immersive project.
//
// 1. A write-only image declared after a PERSISTENT image: after the
//    ping-pong swap the graphics pass that shows the write-only image's outlet
//    was re-pointed at the persistent image, which is the fallback output
//    texture; 'result' showed 'state'.
// 2. A {"TYPE": "cubemap"} RESOURCE was never declared in the generated GLSL,
//    so a compute shader could not read a cube. It is now a samplerCube, read
//    with IMG_CUBE_LOD (compute shaders have no implicit derivatives, so the
//    compute prelude also gets IMG_NORM_PIXEL_LOD).
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <string>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}
}

TEST_CASE(
    "a write-only image declared after a PERSISTENT image publishes its own content",
    "[gfx][csf][persistent]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));
  bool skipped{};
  std::string error;
  std::array<ReadbackImage, 2> shots;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int cs = p.addCsf(corpus("csf-persistent-then-writeonly.cs"));
    if(cs < 0)
    {
      error = p.error();
      return;
    }
    const int sink = p.addSink({16, 16});
    p.wire(p.imageOut(cs, 1), p.sinkInput(sink));
    if(!p.create(be))
    {
      skipped = p.skipped();
      error = skipped ? std::string{} : p.error();
      return;
    }
    p.render(1);
    shots[0] = p.readback(sink);
    p.render(2);
    shots[1] = p.readback(sink);
    if(error.empty())
      error = p.error();
  });
  if(skipped)
    SKIP("backend unavailable");
  if(const char* why = compute_shader_skip_reason(be))
    SKIP(std::string{backend_name(be)} + ": " + why);
  INFO(error);
  REQUIRE(error.empty());
  for(const auto& img : shots)
  {
    REQUIRE(img.valid());
    const auto px = img.at(img.width / 2, img.height / 2);
    INFO(int(px[0]) << "," << int(px[1]) << "," << int(px[2]));
    CHECK(px[0] < 50);
    CHECK(px[1] > 200);
  }
}

TEST_CASE(
    "a compute shader reads a TYPE cubemap resource",
    "[gfx][csf][cubemap]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));
  bool skipped{};
  std::string error;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int prod = p.addRaster(
        corpus("syn-cube-six-colors.vs"), corpus("syn-cube-six-colors.fs"));
    const int cs = p.addCsf(corpus("csf-cubemap-resource.cs"));
    if(prod < 0 || cs < 0)
    {
      error = p.error().empty() ? "node build failed" : p.error();
      return;
    }
    const int sink = p.addSink({96, 64});
    p.wire(p.imageOut(prod, 0), p.imageIn(cs, 0));
    p.wire(p.imageOut(cs, 0), p.sinkInput(sink));
    if(!p.create(be))
    {
      skipped = p.skipped();
      error = skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    img = p.readback(sink);
    if(error.empty())
      error = p.error();
  });
  if(skipped)
    SKIP("backend unavailable");
  if(const char* why = compute_shader_skip_reason(be))
    SKIP(std::string{backend_name(be)} + ": " + why);
  INFO(error);
  REQUIRE(error.empty());
  REQUIRE(img.valid());
  // gl_GlobalInvocationID.y == 0 is the top row on every backend: IMG_STORE
  // flips Y on the non-SPIR-V targets, where the render target is bottom-up.
  static constexpr std::array<std::array<int, 3>, 6> expected{{
      {1, 0, 0}, // +X red
      {0, 1, 1}, // -X cyan
      {0, 1, 0}, // +Y green
      {1, 0, 1}, // -Y magenta
      {0, 0, 1}, // +Z blue
      {1, 1, 0}, // -Z yellow
  }};
  for(int cy = 0; cy < 2; cy++)
    for(int cx = 0; cx < 3; cx++)
    {
      const auto px = img.at(cx * img.width / 3 + img.width / 6, cy * img.height / 2 + img.height / 4);
      const std::array<int, 3> got{px[0] > 128, px[1] > 128, px[2] > 128};
      INFO("cell " << cx << "," << cy << " = " << int(px[0]) << "," << int(px[1]) << ","
                   << int(px[2]));
      CHECK(got == expected[cy * 3 + cx]);
    }
}
