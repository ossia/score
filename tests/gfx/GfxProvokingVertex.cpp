// Flat varyings take the first vertex of a triangle on every backend.
//
// Vulkan, Direct3D and Metal use the first vertex of a triangle as the
// provoking vertex; OpenGL defaults to the last. A shader that carries a
// per-polygon value through a flat varying (an ordering-table depth, a face
// id) therefore gave a different image on OpenGL. Every vertex of the fixture's
// single triangle carries a different colour, so the frame's colour names the
// vertex that was used.
//
// Registration:
//   score_add_gfx_test(provoking_vertex GfxProvokingVertex.cpp)
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}
}

TEST_CASE("flat varyings take the first vertex", "[gfx][raster][flat]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  std::string err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raster = p.addRaster(
        corpus("rr-provoking-vertex.vs"), corpus("rr-provoking-vertex.fs"));
    if(raster < 0)
    {
      err = p.error();
      return;
    }
    const int sink = p.addSink({32, 32});
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
  for(auto [x, y] : std::array<std::array<int, 2>, 3>{{{4, 4}, {16, 16}, {27, 27}}})
  {
    const auto px = img.at(x, y);
    INFO("pixel " << x << "," << y << " = " << int(px[0]) << " " << int(px[1]) << " "
                  << int(px[2]));
    CHECK(px[0] > 200);
    CHECK(px[1] < 40);
    CHECK(px[2] < 40);
  }
}
