// ctest -R gfx_sink_depth_clear_polarity
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Threedim/Primitive.hpp>

#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <halp/controls.hpp>
#include <halp/texture.hpp>

#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

//! Owns the ProcessModels the GfxNodes reference; must outlive the pipeline.
struct HalpProcesses
{
  std::vector<std::unique_ptr<Process::ProcessModel>> models;
  int next = 1;

  template <typename T>
  std::unique_ptr<score::gfx::Node> make(const score::DocumentContext& ctx)
  {
    auto model = std::make_unique<oscr::ProcessModel<T>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{next}, ctx, nullptr);
    auto* raw = model.get();
    models.push_back(std::move(model));
    return std::unique_ptr<score::gfx::Node>{
        new oscr::GfxNode<T>{*raw, {}, Gfx::exec_controls{}, next++, ctx}};
  }
};

struct Run
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  ReadbackImage img;
};

struct Coverage
{
  int count = 0;
  std::vector<uint8_t> mask;
};

Coverage coverage(const ReadbackImage& img)
{
  Coverage c;
  if(!img.valid())
    return c;
  c.mask.resize(std::size_t(img.width) * img.height, 0);
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
    {
      const auto px = img.at(x, y);
      const bool lit = (int(px[0]) + int(px[1]) + int(px[2])) > 128;
      c.mask[std::size_t(y) * img.width + x] = lit ? 1 : 0;
      c.count += lit ? 1 : 0;
    }
  return c;
}

Run render_cube(score::gfx::GraphicsApi api, const char* vs, const char* fs)
{
  Run out;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* document = score::test::new_document(app);
    if(!document)
    {
      out.error = "could not create a document (ProcessModel needs one)";
      return;
    }
    const score::DocumentContext& ctx = document->context();

    HalpProcesses procs;
    GfxPipeline p;

    const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster = p.addRaster(corpus(vs), corpus(fs));
    if(raster < 0)
    {
      out.error = "raster build failed: " + p.error();
      return;
    }

    const int cube = p.addNode(procs.make<Threedim::Cube>(ctx));
    auto* cubeOut = p.nodeSceneOut(cube, 0);
    auto* flatIn = p.nodeSceneIn(flat, 0);
    if(!cubeOut || !flatIn)
    {
      out.error = "scene ports missing on the chain";
      return;
    }
    p.wire(cubeOut, flatIn);
    p.wire(p.nodeGeometryOut(flat, 0), p.geometryIn(raster, 0));

    const int sink = p.addSink({160, 160});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

    if(!p.create(api))
    {
      out.backend = p.backend();
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = p.error();
      return;
    }
    out.backend = p.backend();
    p.render(4);
    out.img = p.readback(sink);
  });
  return out;
}
}

TEST_CASE(
    "a sink pass clears depth for the compare the shader declared",
    "[gfx][l3][depth][sink-depth-clear]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Run rev
      = render_cube(api, "rr-sinkdepth-greater.vs", "rr-sinkdepth-greater.fs");
  const Run fwd = render_cube(api, "rr-sinkdepth-less.vs", "rr-sinkdepth-less.fs");

  if(rev.skipped || fwd.skipped)
  {
    WARN("skipped: " + (rev.skipped ? rev.skip_reason : fwd.skip_reason));
    return;
  }
  INFO("reverse-Z run: " << rev.error);
  INFO("forward-Z run: " << fwd.error);
  REQUIRE(rev.error.empty());
  REQUIRE(fwd.error.empty());
  REQUIRE(rev.img.valid());
  REQUIRE(fwd.img.valid());

  const Coverage cr = coverage(rev.img);
  const Coverage cf = coverage(fwd.img);

  INFO("reverse-Z covered " << cr.count << " px of " << cr.mask.size());
  REQUIRE(cr.count > 200);

  INFO("forward-Z covered " << cf.count << " px of " << cf.mask.size());
  CHECK(cf.count > 200);

  REQUIRE(cr.mask.size() == cf.mask.size());
  std::size_t differing = 0;
  for(std::size_t i = 0; i < cr.mask.size(); ++i)
    differing += (cr.mask[i] != cf.mask[i]) ? 1 : 0;
  INFO("silhouettes differ on " << differing << " px");
  CHECK(differing == 0);
}
