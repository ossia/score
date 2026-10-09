// The `camera` block a raw raster binds before any scene feeds it carries the
// render size. ScenePreprocessor packs camera.renderSize from the render list
// (GfxEnvRenderTargetSize.cpp pins that path); the placeholder bound while no
// scene camera reaches the raster was seeded with identity matrices and a zero
// renderSize, so `gl_FragCoord / camera.renderSize` divided by zero.
#include "IsfTestCommon.hpp"

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <string>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
constexpr int kW = 96;
constexpr int kH = 48;

constexpr const char* kVert = R"(void main()
{
  isf_vertShaderInit();
  int idx = gl_VertexIndex % 3;
  vec2 ndc = vec2((idx & 1) != 0 ? 3.0 : -1.0, (idx & 2) != 0 ? 3.0 : -1.0);
  gl_Position = clipSpaceCorrMatrix * vec4(ndc, 0.0, 1.0);
  isf_vertShaderFinish();
}
)";

// R, G = camera.renderSize / 255 through the injected camera block.
constexpr const char* kFrag = R"(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none",
    "VERTEX_COUNT": 3, "TOPOLOGY": "triangles"
  }
}*/
void main()
{
  vec2 rs = camera.data[0].cameraRenderSize_.xy;
  isf_FragColor = vec4(rs / 255.0, 0.0, 1.0);
}
)";

QString writeText(const QTemporaryDir& dir, const QString& name, const QByteArray& text)
{
  const QString path = dir.filePath(name);
  QFile f(path);
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return {};
  f.write(text);
  return path;
}
}

TEST_CASE(
    "the placeholder camera of an unfed raw raster carries the render size",
    "[gfx][raster][camera]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString vs = writeText(dir, "camrs.vert", kVert);
  const QString fs = writeText(dir, "camrs.frag", kFrag);

  bool skipped = false;
  std::string err;
  ReadbackImage img;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raster = p.addRaster(vs, fs);
    if(raster < 0)
    {
      err = "raster build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({kW, kH});
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
  INFO("error=" << err);
  REQUIRE(err.empty());
  const auto px = img.center();
  CAPTURE(int(px[0]), int(px[1]));
  CHECK(int(px[0]) == kW);
  CHECK(int(px[1]) == kH);
}
