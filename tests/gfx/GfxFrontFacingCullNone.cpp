// gl_FrontFacing under CULL_MODE none.
//
// The generated vertex epilogue mirrors Y on D3D and Metal, and on every
// backend for a pass that renders cube faces. The mirror reverses window-space
// winding, so the raster pipeline swaps its FrontFace to compensate. That swap
// also decides gl_FrontFacing, which two-sided scene materials read to flip
// their normal: a CULL_MODE none pipeline needs it as much as a culled one.
//
// A fullscreen triangle wound counter-clockwise in GL NDC paints green where
// gl_FrontFacing holds and red elsewhere, once into a 2D target and once into
// each face of a cube under EXECUTION_MODEL PER_CUBE_FACE (read back through
// syn-cube-six-probe.fs). The same triangle survives CULL_MODE back on those
// paths (cube-dir-ndc-perface-cull.fs), so front is what it must report.
#include "IsfTestCommon.hpp"

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <string>

using namespace score::test;
using namespace score::test::gfx;
using score::test::gfx::isf::corpus;

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

// `extra` is spliced into the header: the cube OUTPUTS and EXECUTION_MODEL.
QByteArray facingFrag(const char* extra)
{
  return QByteArray(R"(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],)")
         + extra + R"(
  "INPUTS": [],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none",
    "VERTEX_COUNT": 3, "TOPOLOGY": "triangles"
  }
}*/
void main()
{
  isf_FragColor = gl_FrontFacing ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);
}
)";
}

constexpr const char* kCube = R"(
  "OUTPUTS": [ { "NAME": "cube", "TYPE": "color", "FORMAT": "rgba8",
                 "LAYERS": 6, "CUBEMAP": true, "WIDTH": 32, "HEIGHT": 32 } ],
  "EXECUTION_MODEL": { "TYPE": "PER_CUBE_FACE", "TARGET": "cube" },)";

QString writeText(const QTemporaryDir& dir, const QString& name, const QByteArray& text)
{
  const QString path = dir.filePath(name);
  QFile f(path);
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return {};
  f.write(text);
  return path;
}

struct Result
{
  bool skipped = false;
  std::string err;
  ReadbackImage img;
};

Result render(score::gfx::GraphicsApi api, bool cube)
{
  Result r;
  QTemporaryDir dir;
  if(!dir.isValid())
  {
    r.err = "no temporary directory";
    return r;
  }
  const QString vs = writeText(dir, "facing.vert", kVert);
  const QString fs = writeText(dir, "facing.frag", facingFrag(cube ? kCube : ""));
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raster = p.addRaster(vs, fs);
    const int probe = cube ? p.addIsf(corpus("syn-cube-six-probe.fs")) : -1;
    if(raster < 0 || (cube && probe < 0))
    {
      r.err = "chain build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({48, 32});
    if(cube)
    {
      p.wire(p.imageOut(raster, 0), p.imageIn(probe, 0));
      p.wire(p.imageOut(probe, 0), p.sinkInput(sink));
    }
    else
    {
      p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    }
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.err = r.skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    r.img = p.readback(sink);
    if(!r.img.valid())
      r.err = "empty readback";
  });
  return r;
}
}

TEST_CASE(
    "gl_FrontFacing under CULL_MODE none reports a CCW triangle as front",
    "[gfx][raster][cull]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool cube = GENERATE(false, true);
  CAPTURE(backend_name(api), cube);

  const auto r = render(api, cube);
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.err);
  REQUIRE(r.err.empty());

  // The 2D target is one colour; the probe is a 3x2 grid of faces.
  for(int row = 0; row < 2; ++row)
    for(int col = 0; col < 3; ++col)
    {
      const auto px = r.img.at(
          col * r.img.width / 3 + r.img.width / 6,
          row * r.img.height / 2 + r.img.height / 4);
      CAPTURE(row, col, int(px[0]), int(px[1]), int(px[2]));
      CHECK(int(px[0]) < 16);
      CHECK(int(px[1]) > 240);
    }
}
