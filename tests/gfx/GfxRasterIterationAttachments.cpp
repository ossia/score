// A PER_CUBE_FACE / PER_MIP / PER_LAYER raw raster iteration attached only
// the target output, at location 0: the shader's other outputs were dropped,
// and whatever it wrote to location 0 landed in the target, under blend entry
// 0 (shader board R18 / R31). Every colour output keeps its location now.
// Here the cube target is the SECOND output: it must hold what the shader
// writes to that output (green), not to the first one (red).
#include "IsfTestCommon.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

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
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "flat_out" },
    { "TYPE": "vec4", "NAME": "cube_out" }
  ],
  "OUTPUTS": [
    { "NAME": "flat_out", "TYPE": "color", "WIDTH": 32, "HEIGHT": 32 },
    { "NAME": "cube_out", "TYPE": "color", "WIDTH": 32, "HEIGHT": 32, "CUBEMAP": true, "LAYERS": 6 }
  ],
  "EXECUTION_MODEL": { "TYPE": "PER_CUBE_FACE", "TARGET": "cube_out" },
  "INPUTS": [],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none",
    "VERTEX_COUNT": 3, "TOPOLOGY": "triangles"
  }
}*/
void main()
{
  flat_out = vec4(1.0, 0.0, 0.0, 1.0);
  cube_out = vec4(0.0, 1.0, 0.0, 1.0);
}
)";
}

TEST_CASE(
    "a cube-face iteration's target holds its own output", "[gfx][raster][R18][R31]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const auto write = [&](const char* name, const char* text) {
    QFile f(dir.filePath(name));
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(text);
    return dir.filePath(name);
  };
  const QString vs = write("iter.vert", kVert);
  const QString fs = write("iter.frag", kFrag);

  bool skipped = false;
  std::string err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raster = p.addRaster(vs, fs);
    const int view = p.addIsf(score::test::gfx::isf::corpus("syn-cube-six-probe.fs"));
    if(raster < 0 || view < 0)
    {
      err = "build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({60, 40});
    p.wire(p.imageOut(raster, 1), p.imageIn(view, 0));
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    img = p.readback(sink);
  });
  if(skipped)
    SKIP("backend unavailable");
  INFO("error=" << err);
  REQUIRE(err.empty());
  REQUIRE(img.valid());
  // Every face cell of the probe: green.
  int green = 0;
  for(int row = 0; row < 2; row++)
    for(int col = 0; col < 3; col++)
    {
      const auto px = img.at(col * 20 + 10, row * 20 + 10);
      CAPTURE(row, col, int(px[0]), int(px[1]), int(px[2]));
      green += near(px, {0, 255, 0, 255}, 4);
    }
  CHECK(green == 6);
}
