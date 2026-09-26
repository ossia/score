// "POLYGON_MODE": "line" draws a wireframe where the backend supports it, and
// falls back to fill, with one warning, where it does not.
//
// QRhi reports QRhi::NonFillPolygonMode as: always on D3D11 / D3D12 / Metal,
// never on OpenGL ES (glPolygonMode does not exist there, so the request would
// silently fill), and on Vulkan only when the device has fillModeNonSolid
// (setting VK_POLYGON_MODE_LINE without it is invalid usage).
//
// GPU lane: one white triangle per mode, 64x64 sink. Line mode, where
// supported, leaves the centre (inside the triangle) empty and lights only a
// thin outline; fill mode covers the centre and a third of the frame.
// The fallback's warning is pinned in tests/unit/GfxPolygonModeFallbackTest.
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

int litPixels(const ReadbackImage& img)
{
  int n = 0;
  for(int y = 0; y < img.height; y++)
    for(int x = 0; x < img.width; x++)
      if(img.at(x, y)[0] > 128)
        n++;
  return n;
}
}

TEST_CASE("POLYGON_MODE line draws edges only", "[gfx][raster][polygon-mode]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  bool nonFill = false;
  std::string err;
  ReadbackImage line, fill;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int rl = p.addRaster(
        corpus("rr-polygon-mode.vs"), corpus("rr-polygon-mode-line.fs"));
    const int rf = p.addRaster(
        corpus("rr-polygon-mode.vs"), corpus("rr-polygon-mode-fill.fs"));
    if(rl < 0 || rf < 0)
    {
      err = p.error();
      return;
    }
    const int sl = p.addSink({64, 64});
    const int sf = p.addSink({64, 64});
    p.wire(p.imageOut(rl, 0), p.sinkInput(sl));
    p.wire(p.imageOut(rf, 0), p.sinkInput(sf));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    if(auto rs = p.sink(sl)->renderState(); rs && rs->rhi)
      nonFill = rs->rhi->isFeatureSupported(QRhi::NonFillPolygonMode);
    p.render(2);
    line = p.readback(sl);
    fill = p.readback(sf);
    if(!line.valid() || !fill.valid())
      err = "empty readback";
  });
  if(skipped)
    SKIP("backend unavailable");

  INFO("error=" << err);
  REQUIRE(err.empty());

  const int litFill = litPixels(fill);
  const int litLine = litPixels(line);
  INFO("NonFillPolygonMode=" << nonFill << " lit fill=" << litFill << " lit line=" << litLine
                             << " centre line=" << int(line.at(32, 32)[0]));
  CHECK(fill.at(32, 32)[0] > 200);
  CHECK(litFill > 1000);
  if(nonFill)
  {
    CHECK(line.at(32, 32)[0] < 50);
    CHECK(litLine > 60);
    CHECK(litLine < 500);
  }
  else
  {
    CHECK(line.at(32, 32)[0] > 200);
    CHECK(litLine == litFill);
  }
}
