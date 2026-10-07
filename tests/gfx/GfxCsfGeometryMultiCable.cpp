// Two cables into one CSF geometry input. A compute shader reads one geometry
// per input, so it takes the first cable's and warns about the others.
//
// Each producer emits three vertices; the consumer stores the length of its
// position attribute as red / 255.
#include "GfxLogCapture.hpp"
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
constexpr const char* kConsumer = R"(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "WIDTH": "8", "HEIGHT": "8" },
    { "NAME": "geoIn", "TYPE": "geometry",
      "ATTRIBUTES": [ { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_only" } ] }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } } ]
}*/
void main()
{
  ivec2 p = ivec2(gl_GlobalInvocationID.xy);
  IMG_STORE(outputImage, p, vec4(float(ISF_READ(geoIn, position).length()) / 255.0, 1.0, 0.0, 1.0));
}
)";

struct Result
{
  bool skipped = false;
  std::string err;
  ReadbackImage img;
  bool warned = false;
};

Result run(score::gfx::GraphicsApi api, int cables)
{
  Result r;
  QTemporaryDir dir;
  if(!dir.isValid())
  {
    r.err = "no temporary directory";
    return r;
  }
  const QString cs = dir.filePath("consumer.cs");
  {
    QFile f(cs);
    if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
      r.err = "cannot write the consumer";
      return r;
    }
    f.write(kConsumer);
  }
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    LogCapture log;
    GfxPipeline p;
    const int a = p.addCsf(corpus("syn-geo-producer.cs"));
    const int b = p.addCsf(corpus("syn-geo-producer.cs"));
    const int consumer = p.addCsf(cs);
    if(a < 0 || b < 0 || consumer < 0)
    {
      r.err = "node build failed: " + p.error();
      return;
    }
    p.wire(p.geometryOut(a, 0), p.geometryIn(consumer, 0));
    if(cables > 1)
      p.wire(p.geometryOut(b, 0), p.geometryIn(consumer, 0));
    const int sink = p.addSink({8, 8});
    p.wire(p.imageOut(consumer, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.err = r.skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    r.img = p.readback(sink);
    r.warned = log.count({u"geometry input", u"cables"}) > 0;
  });
  return r;
}
}

TEST_CASE(
    "a CSF geometry input with two cables reads the first and warns",
    "[gfx][csf][geometry]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const int cables = GENERATE(1, 2);
  CAPTURE(backend_name(api), cables);

  const auto r = run(api, cables);
  if(r.skipped)
    SKIP("backend unavailable");
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);
  INFO("error=" << r.err);
  REQUIRE(r.err.empty());
  REQUIRE(r.img.valid());
  const auto px = r.img.center();
  CAPTURE(int(px[0]), int(px[1]), r.warned);
  CHECK(int(px[0]) == 3);
  CHECK(r.warned == (cables > 1));
}
