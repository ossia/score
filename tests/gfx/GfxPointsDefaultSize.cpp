// A point drawn by a shader that never writes gl_PointSize covers one pixel.
//
// Vulkan and OpenGL rasterise such a point at size 1; on Metal the size is
// undefined and the points came out as large squares. The fixture draws a 4x4
// grid of points, so at most sixteen pixels may be lit.
//
// Registration:
//   score_add_gfx_test(points_default_size GfxPointsDefaultSize.cpp)
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}
}

TEST_CASE("an unsized point covers one pixel", "[gfx][raster][points]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  std::string err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raster = p.addRaster(
        corpus("rr-points-default-size.vs"), corpus("rr-points-default-size.fs"));
    if(raster < 0)
    {
      err = p.error();
      return;
    }
    const int sink = p.addSink({64, 64});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(2);
    img = p.readback(sink);
    if(!img.valid())
      err = "empty readback";
  });
  if(skipped)
    SKIP("backend unavailable");
  INFO("error=" << err);
  REQUIRE(err.empty());

  int lit = 0;
  for(int y = 0; y < img.height; y++)
    for(int x = 0; x < img.width; x++)
      if(img.at(x, y)[0] > 128)
        lit++;
  INFO("lit pixels " << lit);
  CHECK(lit >= 1);
  CHECK(lit <= 16);
}
