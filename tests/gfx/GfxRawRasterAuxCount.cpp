// $COUNT_<name> / $BYTESIZE_<name> in a raw raster's integer expressions
// resolve from the byte size of the auxiliary buffer the upstream geometry
// publishes under that name.
//
// The producer declares `items` as 24 vec4 elements; the consumer's MANUAL
// invocation count is $COUNT_items, and each invocation redraws the target
// with red = (PASSINDEX + 1) / 255, so the red left on screen is the count.
// A size that stays at the placeholder's, or at 0, evaluates to 1.
//
// Run twice: with two fragment outputs and with one. A single-output shader
// used to take the single-target path, which has no MANUAL invocation loop,
// so its count read 1 whatever $COUNT_ said.
//
// Registration:
//   score_add_gfx_test(raw_raster_aux_count GfxRawRasterAuxCount.cpp)
//
//   DISPLAY=:0 SCORE_TESTS_NO_XVFB=1 SCORE_TEST_API=vulkan \
//     ctest -R gfx_raw_raster_aux_count
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

TEST_CASE(
    "a raw raster's $COUNT_ resolves from the upstream auxiliary's size",
    "[gfx][l3][auxiliary][expression]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  const char* fs = GENERATE("rr-aux-count.fs", "rr-aux-count-single.fs");
  CAPTURE(backend_name(be));
  CAPTURE(fs);

  bool built = false;
  bool skipped = false;
  std::string err;
  std::array<uint8_t, 4> centre{};

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int producer = p.addCsf(corpus("syn-aux-count.cs"));
    const int consumer
        = p.addRaster(corpus("rr-aux-count.vs"), corpus(fs));
    if(producer < 0 || consumer < 0)
    {
      err = "node build failed: " + p.error();
      return;
    }
    p.wire(p.geometryOut(producer, 0), p.geometryIn(consumer, 0));
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(consumer, 0), p.sinkInput(sink));

    if(!p.create(be))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    built = true;
    p.render(5);
    const auto img = p.readback(sink);
    if(!img.valid())
    {
      err = "readback failed";
      return;
    }
    centre = img.at(img.width / 2, img.height / 2);
  });

  if(skipped)
    SKIP("backend unavailable");

  INFO("backend=" << backend_name(be) << " error=" << err);
  INFO(
      "centre=(" << int(centre[0]) << "," << int(centre[1]) << ","
                 << int(centre[2]) << ")");
  REQUIRE(err.empty());
  REQUIRE(built);

  CHECK(int(centre[0]) == 24);
}
