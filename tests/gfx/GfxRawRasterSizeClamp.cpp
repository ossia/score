// A raw raster's own storage images follow the device's TextureSizeMax like
// its render targets: an AUXILIARY image declared wider than any device allows
// is clamped instead of failing to create.
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
    "an oversized aux storage image is clamped to the device limit",
    "[gfx][raw][storage][limits]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  bool skipped = false;
  std::string err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raster = p.addRaster(
        corpus("rr-aux-storage-oversize.vs"), corpus("rr-aux-storage-oversize.fs"));
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
