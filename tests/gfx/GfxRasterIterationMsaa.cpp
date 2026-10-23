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
std::string fragment(
    const std::string& model, const std::string& extra, int samples, bool depth)
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
    "DEPTH_TEST": )_" + std::string(depth ? "true" : "false") + R"_(, "DEPTH_WRITE": )_"
         + std::string(depth ? "true" : "false") + R"_(, "CULL_MODE": "none",
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
    int samples, bool depth, int level = 0)
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
    f.write(QByteArray::fromStdString(fragment(model, extra, samples, depth)));
  }
  ReadbackImage img;
  if(level == 0)
  {
    IsfResult r;
    score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
      r = render_raster(api, {}, corpus("rr-msaa-edge.vs"), fs, {64, 64}, 3);
    });
    e.skipped = r.skipped;
    e.err = r.error;
    if(e.skipped || !e.err.empty())
      return e;
    if(!r.outputs.empty())
      img = r.outputs[0];
  }
  else
  {
    // The level read straight from the target texture, no sampler in the
    // path (as GfxReviewRenderTargets reads a PER_MIP tail level).
    score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
      GfxPipeline p;
      const int raster = p.addRaster(corpus("rr-msaa-edge.vs"), fs);
      const int v = p.addIsf(corpus("msaa-input-view.fs"));
      if(raster < 0 || v < 0)
      {
        e.err = "build failed: " + p.error();
        return;
      }
      const int sink = p.addSink({64, 64});
      p.wire(p.imageOut(raster, 0), p.imageIn(v, 0));
      p.wire(p.imageOut(v, 0), p.sinkInput(sink));
      if(!p.create(api))
      {
        e.skipped = p.skipped();
        e.err = e.skipped ? std::string{} : p.error();
        return;
      }
      p.render(3);
      auto* node = p.isf(raster);
      if(node->renderedNodes.empty())
      {
        e.err = "no raster renderer";
        return;
      }
      auto [list, renderer] = *node->renderedNodes.begin();
      auto* tex = renderer->textureForOutput(*p.imageOut(raster, 0));
      if(!tex || !tex->flags().testFlag(QRhiTexture::MipMapped))
      {
        e.err = "the PER_MIP target has no mip chain";
        return;
      }
      auto& rhi = *list->state.rhi;
      QRhiCommandBuffer* cb{};
      if(rhi.beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
      {
        e.err = "readback frame failed";
        return;
      }
      QRhiReadbackResult rb;
      QRhiReadbackDescription desc(tex);
      desc.setLevel(level);
      auto* updates = rhi.nextResourceUpdateBatch();
      updates->readBackTexture(desc, &rb);
      cb->resourceUpdate(updates);
      rhi.endOffscreenFrame();
      rhi.finish();
      img.width = rb.pixelSize.width();
      img.height = rb.pixelSize.height();
      img.bytes = rb.data;
    });
    if(e.skipped || !e.err.empty())
      return e;
  }
  if(!img.valid())
  {
    e.err = "empty readback";
    return e;
  }
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
  // With a depth test, the iteration's depth renders multisampled too.
  const bool depth = GENERATE(false, true);
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(m.type, depth, backend_name(api));

  const auto msaa = renderEdge(api, m.type, m.extra, 4, depth);
  if(msaa.skipped)
    SKIP("backend unavailable");
  INFO("SAMPLES 4: " << msaa.err << " partial=" << msaa.partial << " lit=" << msaa.lit);
  REQUIRE(msaa.err.empty());
  const auto single = renderEdge(api, m.type, m.extra, 1, depth);
  INFO("SAMPLES 1: " << single.err << " partial=" << single.partial << " lit=" << single.lit);
  REQUIRE(single.err.empty());

  CHECK(msaa.lit > 500);
  CHECK(single.lit > 500);
  CHECK(msaa.partial > 20);
  CHECK(single.partial == 0);
}

// The resolve lands in the iterated level itself, not only level 0: level 1
// of a PER_MIP target (32x32) has the same antialiased edge.
TEST_CASE("a PER_MIP iteration resolves into each level", "[gfx][raster][msaa][R31]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const auto msaa = renderEdge(api, "PER_MIP", "", 4, false, 1);
  if(msaa.skipped)
    SKIP("backend unavailable");
  INFO("SAMPLES 4, level 1: " << msaa.err << " partial=" << msaa.partial << " lit=" << msaa.lit);
  REQUIRE(msaa.err.empty());
  const auto single = renderEdge(api, "PER_MIP", "", 1, false, 1);
  INFO("SAMPLES 1, level 1: " << single.err << " partial=" << single.partial << " lit=" << single.lit);
  REQUIRE(single.err.empty());
  CHECK(msaa.lit > 120);
  CHECK(single.lit > 120);
  CHECK(msaa.partial > 8);
  CHECK(single.partial == 0);
}
