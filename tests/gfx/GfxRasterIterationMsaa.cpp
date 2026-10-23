// A PER_MIP / PER_LAYER / PER_CUBE_FACE raw raster iteration rendered its
// target single-sampled, whatever the OUTPUT's SAMPLES: the per-iteration
// render targets were built with one sample (shader board R31). SAMPLES is
// honoured in the iterations now: each one renders multisampled and resolves
// into its level / layer / face. The same antialiased edge as
// GfxRawRasterOutputAndVertexInput's MSAA test, drawn through an iteration.
#include "IsfTestCommon.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <string>

using namespace score::test::gfx;
using score::test::gfx::isf::corpus;

namespace
{
std::string fragment(const std::string& model, const std::string& extra, int samples)
{
  return R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "OUTPUTS": [ { "NAME": "target", "TYPE": "color", "FORMAT": "rgba8",
                 "WIDTH": 64, "HEIGHT": 64, "SAMPLES": )_"
         + std::to_string(samples) + extra + R"_( } ],
  "EXECUTION_MODEL": { "TYPE": ")_" + model + R"_(", "TARGET": "target" },
  "INPUTS": [],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none",
    "TOPOLOGY": "triangles", "VERTEX_COUNT": 3
  }
}*/
void main() { isf_FragColor = vec4(1.0); }
)_";
}

struct Edge
{
  bool skipped = false;
  std::string err;
  int partial = 0;
  int lit = 0;
};

Edge renderEdge(
    score::gfx::GraphicsApi api, const std::string& model, const std::string& extra,
    int samples)
{
  QTemporaryDir dir;
  Edge e;
  if(!dir.isValid())
  {
    e.err = "no temporary directory";
    return e;
  }
  const QString fs = dir.filePath("iter-msaa.fs");
  {
    QFile f{fs};
    if(!f.open(QIODevice::WriteOnly))
    {
      e.err = "cannot write the shader";
      return e;
    }
    f.write(QByteArray::fromStdString(fragment(model, extra, samples)));
  }
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_raster(
        api, {}, corpus("rr-msaa-edge.vs"), fs, {64, 64}, 3);
  });
  e.skipped = r.skipped;
  e.err = r.error;
  if(e.skipped || !e.err.empty())
    return e;
  if(r.outputs.empty() || !r.outputs[0].valid())
  {
    e.err = "empty readback";
    return e;
  }
  const auto& img = r.outputs[0];
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
    {
      const int v = img.at(x, y)[0];
      if(v > 24 && v < 232)
        ++e.partial;
      if(v >= 232)
        ++e.lit;
    }
  return e;
}
}

TEST_CASE("a raw raster iteration honours its target's SAMPLES", "[gfx][raster][msaa][R31]")
{
  struct Model
  {
    std::string type, extra;
  };
  const auto m = GENERATE(
      Model{"PER_MIP", ""}, Model{"PER_LAYER", R"(, "LAYERS": 2)"});
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(m.type, backend_name(api));

  const auto msaa = renderEdge(api, m.type, m.extra, 4);
  if(msaa.skipped)
    SKIP("backend unavailable");
  INFO("SAMPLES 4: " << msaa.err << " partial=" << msaa.partial << " lit=" << msaa.lit);
  REQUIRE(msaa.err.empty());
  const auto single = renderEdge(api, m.type, m.extra, 1);
  INFO("SAMPLES 1: " << single.err << " partial=" << single.partial << " lit=" << single.lit);
  REQUIRE(single.err.empty());

  CHECK(msaa.lit > 500);
  CHECK(single.lit > 500);
  CHECK(msaa.partial > 20);
  CHECK(single.partial == 0);
}
