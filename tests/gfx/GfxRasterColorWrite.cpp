// A top-level PIPELINE_STATE.COLOR_WRITE masks the colour writes of a raw
// raster, as the COLOR_WRITE inside BLEND does. The parser only knew the BLEND
// one, so the top-level key was dropped and every channel was written.
//
// A fullscreen triangle writes white with COLOR_WRITE "g": only green reaches
// the target.
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
constexpr const char* kVert = R"(void main()
{
  isf_vertShaderInit();
  int idx = gl_VertexIndex % 3;
  vec2 ndc = vec2((idx & 1) != 0 ? 3.0 : -1.0, (idx & 2) != 0 ? 3.0 : -1.0);
  gl_Position = clipSpaceCorrMatrix * vec4(ndc, 0.0, 1.0);
  isf_vertShaderFinish();
}
)";

constexpr const char* kFrag = R"(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none",
    "VERTEX_COUNT": 3, "TOPOLOGY": "triangles", "COLOR_WRITE": "g"
  }
}*/
void main() { isf_FragColor = vec4(1.0); }
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

TEST_CASE("a top-level COLOR_WRITE masks a raw raster's writes", "[gfx][raster][blend]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString vs = writeText(dir, "colorwrite.vert", kVert);
  const QString fs = writeText(dir, "colorwrite.frag", kFrag);

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
    const int sink = p.addSink({16, 16});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(2);
    img = p.readback(sink);
    if(err.empty())
      err = p.error();
  });
  if(skipped)
    SKIP("backend unavailable");
  INFO("error=" << err);
  REQUIRE(err.empty());
  REQUIRE(img.valid());
  const auto px = img.center();
  CAPTURE(int(px[0]), int(px[1]), int(px[2]), int(px[3]));
  CHECK(int(px[0]) == 0);
  CHECK(int(px[1]) == 255);
  CHECK(int(px[2]) == 0);
}
