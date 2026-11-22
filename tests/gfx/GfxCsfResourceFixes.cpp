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
