// A raw raster AUXILIARY SIZE expression reading an input sizes the buffer
// bound while nothing upstream publishes it. Any non-numeric SIZE used to
// allocate 1024 elements, which $COUNT_<name> then reported.
//
// `items` is unbound and sized "$n" with n = 2; VERTEX_COUNT is
// "$COUNT_items * 3", and triangle k covers the centre of the k-th of four
// vertical stripes, so exactly the first two stripes are lit.
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

constexpr const char* kVert = R"(void main()
{
  isf_vertShaderInit();
  int k = gl_VertexIndex / 3;
  int idx = gl_VertexIndex % 3;
  float x0 = -1.0 + 0.5 * float(k);
  vec2 p = idx == 0 ? vec2(x0, -1.0) : idx == 1 ? vec2(x0 + 0.5, -1.0) : vec2(x0 + 0.25, 3.0);
  gl_Position = clipSpaceCorrMatrix * vec4(p, 0.0, 1.0);
  isf_vertShaderFinish();
}
)";

constexpr const char* kFrag = R"(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [ { "NAME": "n", "TYPE": "long", "DEFAULT": 2, "MIN": 1, "MAX": 4 } ],
  "AUXILIARY": [
    { "NAME": "items", "ACCESS": "read_only", "SIZE": "$n",
      "LAYOUT": [ { "NAME": "values", "TYPE": "vec4[]" } ] }
  ],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none", "TOPOLOGY": "triangles",
    "VERTEX_COUNT": "$COUNT_items * 3"
  }
}*/
void main() { isf_FragColor = vec4(0.0, 1.0 + 0.0 * items.values[0].x, 0.0, 1.0); }
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
    "an unbound raw raster AUXILIARY sized by an input expression has that size",
    "[gfx][raster][auxiliary][expression]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString vs = writeText(dir, "auxsize.vert", kVert);
  const QString fs = writeText(dir, "auxsize.frag", kFrag);

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
  std::array<bool, 4> lit{};
  for(int k = 0; k < 4; ++k)
    lit[k] = img.at(k * kW / 4 + kW / 8, kH / 2)[1] > 128;
  CAPTURE(lit[0], lit[1], lit[2], lit[3]);
  CHECK(lit == std::array<bool, 4>{true, true, false, false});
}
