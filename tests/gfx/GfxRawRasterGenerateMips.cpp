// GENERATE_MIPS on a raw raster's one plain colour output (shader board R32).
// Such an output was drawn on the direct path, into the render target its
// consumer provides, which has no mip chain: the request was dropped. It now
// takes the node's own target, allocated mipmapped, and its mips are generated.
//
// rr-generate-mips-single.fs (left red, right green) -> isf-static-lod-consumer
// (binds the producer's texture, samples LOD 10): the average, not red.
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
    "GENERATE_MIPS on a lone raw raster output generates its mips",
    "[gfx][raw][mips][R32]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  bool skipped = false;
  std::string err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raster = p.addRaster(
        corpus("rr-generate-mips-single.vs"), corpus("rr-generate-mips-single.fs"));
    const int consumer = p.addIsf(corpus("isf-static-lod-consumer.fs"));
    if(raster < 0 || consumer < 0)
    {
      err = p.error();
      return;
    }
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(raster, 0), p.imageIn(consumer, 0));
    p.wire(p.imageOut(consumer, 0), p.sinkInput(sink));
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
  INFO(err);
  REQUIRE(err.empty());
  const auto c = img.at(16, 16);
  INFO("rgb=" << int(c[0]) << "," << int(c[1]) << "," << int(c[2]));
  CHECK(near(c, {128, 128, 0, 255}, 12));
}
