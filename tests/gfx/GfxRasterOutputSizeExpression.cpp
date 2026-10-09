// A raw raster OUTPUTS.WIDTH / HEIGHT expression reading an input sizes the
// target, and follows the input when it changes.
//
// The expression evaluator walked the node's ports one per descriptor input
// from port 0, but port 0 of a raw raster is its Geometry input and an input
// may own no port or several, so `$size` read another port's value and the
// output fell back to the render size. A single plain colour output with an
// explicit size also took the single-target path, which renders at the
// consumer's size whatever OUTPUTS declares.
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

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

QByteArray sizedFrag(bool cube)
{
  return QByteArray(R"(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "OUTPUTS": [ { "NAME": "out", "TYPE": "color", "WIDTH": "$size", "HEIGHT": "$size")")
         + (cube ? R"(, "CUBEMAP": true, "LAYERS": 6 } ],
  "EXECUTION_MODEL": { "TYPE": "PER_CUBE_FACE", "TARGET": "out" },)"
                 : R"( } ],)")
         + R"(
  "INPUTS": [
    { "NAME": "gain", "TYPE": "float", "DEFAULT": 0.5 },
    { "NAME": "size", "TYPE": "long", "DEFAULT": 32, "MIN": 1, "MAX": 512 }
  ],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none",
    "VERTEX_COUNT": 3, "TOPOLOGY": "triangles"
  }
}*/
void main() { isf_FragColor = vec4(gain, 0.0, 0.0, 1.0); }
)";
}

QString writeText(const QTemporaryDir& dir, const QString& name, const QByteArray& text)
{
  const QString path = dir.filePath(name);
  QFile f(path);
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return {};
  f.write(text);
  return path;
}

QSize outputSize(GfxPipeline& p, int raster)
{
  auto& node = *p.isf(raster);
  for(auto& [rl, renderer] : node.renderedNodes)
    if(renderer)
      if(auto* tex = renderer->textureForOutput(*p.imageOut(raster, 0)))
        return tex->pixelSize();
  return {};
}
}

TEST_CASE(
    "a raw raster OUTPUTS size expression follows the input it reads",
    "[gfx][raster][outputs]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool cube = GENERATE(false, true);
  CAPTURE(backend_name(api), cube);

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString vs = writeText(dir, "sized.vert", kVert);
  const QString fs = writeText(dir, "sized.frag", sizedFrag(cube));

  bool skipped = false;
  std::string err;
  QSize initial, resized;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raster = p.addRaster(vs, fs);
    if(raster < 0)
    {
      err = "raster build failed: " + p.error();
      return;
    }
    const int view
        = cube ? p.addIsf(score::test::gfx::isf::corpus("syn-cube-six-probe.fs")) : -1;
    const int sink = p.addSink({64, 64});
    if(cube)
    {
      p.wire(p.imageOut(raster, 0), p.imageIn(view, 0));
      p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    }
    else
    {
      p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    }
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    // Controls: the eight injected mode / blend ones, then gain, then size.
    const int sizePort = nth_control_input(*p.isf(raster), 9);
    p.render(2);
    initial = outputSize(p, raster);
    setControl(*p.isf(raster), sizePort, ossia::value{48});
    p.render(2);
    resized = outputSize(p, raster);
    if(err.empty())
      err = p.error();
  });
  if(skipped)
    SKIP("backend unavailable");
  INFO("error=" << err);
  REQUIRE(err.empty());
  CAPTURE(initial.width(), initial.height(), resized.width(), resized.height());
  CHECK(initial == QSize(32, 32));
  CHECK(resized == QSize(48, 48));
}
