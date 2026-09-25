// A raw raster samples a PER_LAYER depth array through an IS_ARRAY image input.
//
// rr-perlayer-depth writes depths 0.2 / 0.4 / 0.6 / 0.8 into the four layers of
// a 64x64 D32F Texture2DArray (see GfxPerLayerDepth.cpp for the closed form).
// e3-rr-depth-array-probe reads it back from a raw raster and paints
// red = layer 1 depth (102), green = layer count / 8 (4 -> 128) and
// blue = width / 256 (64 -> 64).
//
// The input's sampler must be the one its INPUTS entry declares. The raw
// raster's port 0 is the geometry input, so the declared sampler configs were
// matched one port too early and the input fell back to a mipmapping sampler.
// OpenGL treats a single-level texture under a mipmapping min filter as
// incomplete and samples a 1x1x1 fallback instead: green 32, red and blue 0.
// Vulkan clamps to the view's level count and never showed it.
//
// Registration: see the test_gfx_rawraster_depth_array_input_e3 target.
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cstdlib>
#include <string>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

struct Outcome
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  ReadbackImage view;
};

Outcome run(score::gfx::GraphicsApi api)
{
  Outcome out;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int prod = p.addRaster(
        corpus("rr-perlayer-depth.vs"), corpus("rr-perlayer-depth.fs"));
    const int probe = p.addRaster(
        corpus("e3-rr-depth-array-probe.vs"), corpus("e3-rr-depth-array-probe.fs"));
    if(prod < 0 || probe < 0)
    {
      out.error = "chain build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({16, 16});
    p.wire(p.imageOut(prod, 0), p.imageIn(probe, 0));
    p.wire(p.imageOut(probe, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    out.view = p.readback(sink);
    if(!out.view.valid())
      out.error = "empty readback";
  });
  return out;
}
}

TEST_CASE(
    "a raw raster samples a per-layer depth array through its image input",
    "[gfx][rawraster][depth][array]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Outcome out = run(api);
  if(out.skipped)
    SKIP(out.skip_reason);
  REQUIRE(out.error.empty());

  const auto px = out.view.center();
  CAPTURE(int(px[0]), int(px[1]), int(px[2]));
  CHECK(std::abs(int(px[1]) - 128) <= 2);
  CHECK(std::abs(int(px[2]) - 64) <= 2);
  CHECK(std::abs(int(px[0]) - 102) <= 3);
}
