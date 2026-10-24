// A raw raster declaring CULL_DISTANCES: MSL has no cull distances, so on Metal
// the shader is refused, with one warning, and draws nothing (9f0ac89b96).
// The refusal came after initMRTPass had allocated every iteration target:
// here a PER_LAYER, SAMPLES 4 output, whose multisample and depth
// attachments were made for nothing. A refused shader allocates none, and
// publishes no target its consumers would read uninitialised. Elsewhere the
// shader draws (and allocates them): the positive control.
//
// Registration:
//   score_add_gfx_test(raw_raster_cull_distances GfxRawRasterCullDistances.cpp)
#include "GfxLogCapture.hpp"
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/RenderedRawRasterPipelineNode.hpp>

#include <QFile>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

namespace
{
constexpr const char* kVert = R"_(const vec2 corners[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
void main()
{
    isf_vertShaderInit();
    gl_Position = vec4(corners[gl_VertexIndex % 3], 0.5, 1.0);
    gl_CullDistance[0] = 1.0;
    isf_vertShaderFinish();
}
)_";

constexpr const char* kFrag = R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "CULL_DISTANCES": 1,
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "OUTPUTS": [ { "NAME": "target", "TYPE": "color", "FORMAT": "rgba8",
                 "WIDTH": 32, "HEIGHT": 32, "SAMPLES": 4, "LAYERS": 2 } ],
  "EXECUTION_MODEL": { "TYPE": "PER_LAYER", "TARGET": "target" },
  "INPUTS": [],
  "PIPELINE_STATE": { "DEPTH_TEST": true, "DEPTH_WRITE": true, "CULL_MODE": "none",
                      "TOPOLOGY": "triangles", "VERTEX_COUNT": 3 }
}*/
void main() { isf_FragColor = vec4(1.0); }
)_";
}

TEST_CASE(
    "a raw raster the backend refuses allocates no iteration targets",
    "[gfx][raster][metal][R48]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString vs = dir.filePath("cull.vs"), fs = dir.filePath("cull.fs");
  for(auto [path, text] : {std::pair{vs, kVert}, std::pair{fs, kFrag}})
  {
    QFile f{path};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(text);
  }

  bool skipped = false;
  std::string err;
  int refusals = 0;
  const auto before = score::gfx::iterationAttachmentAllocations().load();
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    LogCapture log;
    GfxPipeline p;
    const int raster = p.addRaster(vs, fs);
    if(raster < 0)
    {
      err = "build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    refusals = log.count({u"CULL_DISTANCES has no Metal equivalent"});
  });
  if(skipped)
    SKIP("backend unavailable");
  INFO(err);
  REQUIRE(err.empty());
  const auto allocated = score::gfx::iterationAttachmentAllocations().load() - before;
  INFO("iteration attachments allocated: " << allocated);
  if(api == score::gfx::Metal)
  {
    CHECK(refusals == 1);
    CHECK(allocated == 0);
  }
  else
  {
    CHECK(refusals == 0);
    CHECK(allocated > 0);
  }
}
