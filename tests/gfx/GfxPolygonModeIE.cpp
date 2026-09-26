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
// CPU lane: supportedPolygonMode keeps Line when supported, turns it into Fill
// when not, and warns once.
//
// Registration: see test_gfx_polygon_mode_ie.
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/PipelineStateHelpers.hpp>

#include <QStringList>

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

QStringList g_warnings;
QtMessageHandler g_previous{};
void collect(QtMsgType t, const QMessageLogContext& c, const QString& msg)
{
  if(t == QtWarningMsg)
    g_warnings.push_back(msg);
  if(g_previous)
    g_previous(t, c, msg);
}
}

TEST_CASE("POLYGON_MODE line draws edges only", "[gfx][raster][polygon-mode][ie]")
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
        corpus("rr-polygon-mode-ie.vs"), corpus("rr-polygon-mode-line-ie.fs"));
    const int rf = p.addRaster(
        corpus("rr-polygon-mode-ie.vs"), corpus("rr-polygon-mode-fill-ie.fs"));
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

TEST_CASE(
    "an unsupported non-fill polygon mode falls back to fill and warns once",
    "[gfx][polygon-mode][ie]")
{
  using PM = QRhiGraphicsPipeline::PolygonMode;
  g_warnings.clear();
  g_previous = qInstallMessageHandler(collect);
  const PM supported = score::gfx::supportedPolygonMode(PM::Line, true);
  const PM fillAlways = score::gfx::supportedPolygonMode(PM::Fill, false);
  const int warningsBefore = int(g_warnings.size());
  const PM first = score::gfx::supportedPolygonMode(PM::Line, false);
  const PM second = score::gfx::supportedPolygonMode(PM::Line, false);
  qInstallMessageHandler(g_previous);
  g_previous = {};

  CHECK(supported == PM::Line);
  CHECK(fillAlways == PM::Fill);
  CHECK(warningsBefore == 0);
  CHECK(first == PM::Fill);
  CHECK(second == PM::Fill);
  REQUIRE(g_warnings.size() == 1);
  CHECK(g_warnings.front().contains("POLYGON_MODE"));
}
