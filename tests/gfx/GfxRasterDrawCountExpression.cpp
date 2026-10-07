// PIPELINE_STATE.VERTEX_COUNT / INSTANCE_COUNT given as expressions reading an
// input are evaluated every frame, so a procedural draw follows the input.
//
// Triangle k (by vertex, or by instance) covers the centre of the k-th of four
// vertical stripes. With `count` = 1 only the first stripe is lit; set to 3, the
// first three are and the last is not. Both counts were integer literals only.
#include "IsfTestCommon.hpp"

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <string>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
constexpr int kW = 64;
constexpr int kH = 16;

QByteArray stripesVert(bool instanced)
{
  return QByteArray(R"(void main()
{
  isf_vertShaderInit();
  int k = )") + (instanced ? "gl_InstanceIndex" : "gl_VertexIndex / 3")
         + R"(;
  int idx = gl_VertexIndex % 3;
  float x0 = -1.0 + 0.5 * float(k);
  vec2 p = idx == 0 ? vec2(x0, -1.0) : idx == 1 ? vec2(x0 + 0.5, -1.0) : vec2(x0 + 0.25, 3.0);
  gl_Position = clipSpaceCorrMatrix * vec4(p, 0.0, 1.0);
  isf_vertShaderFinish();
}
)";
}

QByteArray stripesFrag(bool instanced)
{
  return QByteArray(R"(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [ { "NAME": "count", "TYPE": "long", "DEFAULT": 1, "MIN": 0, "MAX": 4 } ],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none", "TOPOLOGY": "triangles",
    )")
         + (instanced ? R"("VERTEX_COUNT": 3, "INSTANCE_COUNT": "$count")"
                      : R"("VERTEX_COUNT": "$count * 3")")
         + R"(
  }
}*/
void main() { isf_FragColor = vec4(0.0, 1.0, 0.0, 1.0); }
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

std::array<bool, 4> litStripes(const ReadbackImage& img)
{
  std::array<bool, 4> lit{};
  for(int k = 0; k < 4; ++k)
    lit[k] = img.at(k * kW / 4 + kW / 8, kH / 2)[1] > 128;
  return lit;
}
}

TEST_CASE(
    "a procedural raster draw count expression follows the input it reads",
    "[gfx][raster][procedural]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool instanced = GENERATE(false, true);
  CAPTURE(backend_name(api), instanced);

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString vs = writeText(dir, "stripes.vert", stripesVert(instanced));
  const QString fs = writeText(dir, "stripes.frag", stripesFrag(instanced));

  bool skipped = false;
  std::string err;
  ReadbackImage one, three;
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
    p.render(2);
    one = p.readback(sink);
    // Controls: the eight injected mode / blend ones, then count.
    setControl(*p.isf(raster), nth_control_input(*p.isf(raster), 8), ossia::value{3});
    p.render(2);
    three = p.readback(sink);
    if(err.empty())
      err = p.error();
  });
  if(skipped)
    SKIP("backend unavailable");
  INFO("error=" << err);
  REQUIRE(err.empty());
  REQUIRE(one.valid());
  REQUIRE(three.valid());
  const auto a = litStripes(one);
  const auto b = litStripes(three);
  CAPTURE(a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3]);
  CHECK(a == std::array<bool, 4>{true, false, false, false});
  CHECK(b == std::array<bool, 4>{true, true, true, false});
}
