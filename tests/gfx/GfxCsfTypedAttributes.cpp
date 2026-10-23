// A CSF geometry attribute whose type has no vertex format -- a TYPES
// struct, a bool, a matrix -- is published as what it is (shader board C42,
// points 2-5). It used to be published as float4 with no type name, so:
//  * a compute consumer declaring another struct of the same size adopted
//    the bytes and reinterpreted them;
//  * a raster vertex input matched it and fetched it with the format the
//    enum value happened to cast to.
// Now it is user_struct with its type name and std430 element size. A
// compute consumer reads it only as the same type, and is otherwise refused
// with a warning and reads zeros. A raster vertex input is refused. The
// matching compute chains (struct, bool, mat4) keep working.
//
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_csf_typed_attributes
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_csf_typed_attributes
#include "GfxLogCapture.hpp"
#include "IsfTestCommon.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
struct Shot
{
  bool skipped = false;
  std::string error;
  ReadbackImage img;
  int refusals = 0;
};

// producer -> consumer -> sink. A CSF consumer's image output, or a raster's.
Shot run(score::gfx::GraphicsApi api, const QString& consumer, const QString& rasterVs = {})
{
  Shot r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    LogCapture log;
    GfxPipeline p;
    const int prod = p.addCsf(corpus("syn-c42-producer.cs"));
    const int cons = rasterVs.isEmpty() ? p.addCsf(consumer) : p.addRaster(rasterVs, consumer);
    if(prod < 0 || cons < 0)
    {
      r.error = "build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({16, 16});
    p.wire(p.geometryOut(prod, 0), p.geometryIn(cons, 0));
    p.wire(p.imageOut(cons, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    r.img = p.readback(sink);
    r.refusals = log.count(QtWarningMsg, {u"is not converted"})
                 + log.count(QtWarningMsg, {u"has no vertex format"});
  });
  return r;
}
}

TEST_CASE("a struct attribute is read as its own type", "[gfx][csf][geometry][C42]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const Shot r = run(api, corpus("syn-c42-reader.cs"));
  if(r.skipped)
    SKIP("backend unavailable");
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.img.valid());
  const auto c = r.img.center();
  INFO("center " << int(c[0]) << "," << int(c[1]) << "," << int(c[2]));
  CHECK(c[0] < 16);
  CHECK(c[1] > 240);
  CHECK(r.refusals == 0);
}

TEST_CASE(
    "a struct attribute is not reinterpreted as another struct of the same size",
    "[gfx][csf][geometry][C42]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const Shot r = run(api, corpus("syn-c42-other-reader.cs"));
  if(r.skipped)
    SKIP("backend unavailable");
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.img.valid());
  const auto c = r.img.center();
  INFO("center " << int(c[0]) << "," << int(c[1]) << "," << int(c[2]));
  // Zeros, not the producer's green.
  CHECK(c[1] < 16);
  // One warning, not one per frame.
  CHECK(r.refusals == 1);
}

TEST_CASE("bool and mat4 attributes are read as bool and mat4", "[gfx][csf][geometry][C42]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const Shot r = run(api, corpus("syn-c42-flag-reader.cs"));
  if(r.skipped)
    SKIP("backend unavailable");
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.img.valid());
  const auto c = r.img.center();
  INFO("center " << int(c[0]) << "," << int(c[1]) << "," << int(c[2]));
  CHECK(c[0] < 16);
  CHECK(c[1] > 240);
  CHECK(c[2] > 240);
  CHECK(r.refusals == 0);
}

TEST_CASE("a raster vertex input refuses a bool attribute", "[gfx][raster][geometry][C42]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const Shot r = run(api, corpus("syn-c42-flag-raster.fs"), corpus("syn-c42-flag-raster.vs"));
  if(r.skipped)
    SKIP("backend unavailable");
  INFO(r.error);
  REQUIRE(r.error.empty());
  CHECK(r.refusals >= 1);
  if(r.img.valid())
  {
    // The pipeline is refused, so nothing is drawn.
    const auto c = r.img.center();
    CHECK(c[0] < 16);
  }
}
