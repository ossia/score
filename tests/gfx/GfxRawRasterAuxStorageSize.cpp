// A writable AUXILIARY storage image that no upstream produces is allocated by
// the node at its declared WIDTH / HEIGHT (shader board R46). It was a 1 x 1
// placeholder, so imageSize() read 1 x 1 and every store past (0, 0) was lost.
//
// rr-aux-storage-size.fs declares a 7 x 5 `scratch` image and paints green
// when imageSize() agrees.
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
    "a producerless writable aux storage image has its declared size",
    "[gfx][raw][storage][R46]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  bool skipped = false;
  std::string err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raster
        = p.addRaster(corpus("rr-aux-storage-size.vs"), corpus("rr-aux-storage-size.fs"));
    if(raster < 0)
    {
      err = p.error();
      return;
    }
    const int sink = p.addSink({16, 16});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    img = p.readback(sink);
    if(!img.valid())
      err = "empty readback";
  });
  if(skipped)
    SKIP("backend unavailable");
  if(const char* why = storage_buffer_skip_reason(api))
    SKIP(why);
  INFO(err);
  REQUIRE(err.empty());
  CHECK(near(img.at(8, 8), {0, 255, 0, 255}, 2));
}
