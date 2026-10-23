// Raw raster draw counts on a mesh (shader board R21). INSTANCE_COUNT without
// VERTEX_COUNT multiplies the instance count of the mesh's draws; it was read
// only next to VERTEX_COUNT, so a mesh-reading shader asking for 4 instances
// drew 1. VERTEX_COUNT is for procedural draws (VERTEX_INPUTS []); next to
// vertex inputs it is refused (tests/unit/IsfHeaderValidationTest.cpp).
//
// syn-geo-producer.cs (one triangle) -> rr-instance-count-mesh: each instance
// shrinks the triangle into one quadrant, so all 4 quadrant centres are lit.
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* f)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + f;
}
}

TEST_CASE(
    "INSTANCE_COUNT multiplies a raw raster mesh draw", "[gfx][raw][instancing][R21]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  std::string skip, err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    if(const char* why = compute_shader_skip_reason(api))
    {
      skip = why;
      return;
    }
    GfxPipeline p;
    const int geo = p.addCsf(corpus("syn-geo-producer.cs"));
    const int raster = p.addRaster(
        corpus("rr-instance-count-mesh.vs"), corpus("rr-instance-count-mesh.fs"));
    if(geo < 0 || raster < 0)
    {
      err = p.error();
      return;
    }
    const int sink = p.addSink({64, 64});
    p.wire(p.geometryOut(geo, 0), p.geometryIn(raster, 0));
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skip = p.skipped() ? p.skipReason() : std::string{};
      err = p.skipped() ? std::string{} : p.error();
      return;
    }
    p.render(3);
    img = p.readback(sink);
    if(!img.valid())
      err = "empty readback";
  });
  if(!skip.empty())
    SKIP(skip);
  INFO(err);
  REQUIRE(err.empty());
  int lit = 0;
  for(auto [x, y] : {std::pair{16, 16}, {48, 16}, {16, 48}, {48, 48}})
    lit += img.at(x, y)[0] > 128;
  CHECK(lit == 4);
}
