// A scene filter that rewrites indirect_draw_cmds on one branch removes the
// draws from that branch's rasterizer only (agent E4).
//
// Cube -> Scene Preprocessor -+-> csf-e4-scene-filter-zero.cs -> raster A
//                             +-> raster B
//
// The filter zeroes instanceCount of every command in its private copy of the
// preprocessor's indirect_draw_cmds auxiliary. Its output geometry must then
// carry that copy as its indirect-draw buffer, and no longer the preprocessor's
// CPU command list: A paints nothing while B, fed by the same preprocessor,
// still paints the cube. Pinned on the GPU indirect path and on the CPU
// readback rung (SCORE_GFX_NO_GPU_INDIRECT).
//
// Registration: see the test_gfx_scene_filter_indirect_e4 block in CMakeLists.txt.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Threedim/Primitive.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

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

int litPixels(const ReadbackImage& img)
{
  int n = 0;
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
    {
      const auto px = img.at(x, y);
      n += (int(px[0]) + int(px[1]) + int(px[2])) > 128 ? 1 : 0;
    }
  return n;
}

struct Run
{
  bool skipped = false;
  std::string skip_reason, error;
  ReadbackImage filtered, unfiltered;
};

Run render(score::gfx::GraphicsApi api)
{
  Run out;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* document = score::test::new_document(app);
    if(!document)
    {
      out.error = "could not create a document";
      return;
    }
    const score::DocumentContext& ctx = document->context();

    HalpProcesses procs;
    GfxPipeline p;

    const int prep = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int filter = p.addCsf(corpus("csf-e4-scene-filter-zero.cs"));
    const int rasterA
        = p.addRaster(corpus("rr-sinkdepth-less.vs"), corpus("rr-sinkdepth-less.fs"));
    const int rasterB
        = p.addRaster(corpus("rr-sinkdepth-less.vs"), corpus("rr-sinkdepth-less.fs"));
    if(filter < 0 || rasterA < 0 || rasterB < 0)
    {
      out.error = "node build failed: " + p.error();
      return;
    }

    const int cube = p.addNode(procs.make<Threedim::Cube>(ctx));
    auto* cubeOut = p.nodeSceneOut(cube, 0);
    auto* prepIn = p.nodeSceneIn(prep, 0);
    if(!cubeOut || !prepIn || !p.geometryIn(filter, 0) || !p.geometryOut(filter, 0))
    {
      out.error = "ports missing on the chain";
      return;
    }
    p.wire(cubeOut, prepIn);
    p.wire(p.nodeGeometryOut(prep, 0), p.geometryIn(filter, 0));
    p.wire(p.geometryOut(filter, 0), p.geometryIn(rasterA, 0));
    p.wire(p.nodeGeometryOut(prep, 0), p.geometryIn(rasterB, 0));

    const int sinkA = p.addSink({96, 96});
    const int sinkB = p.addSink({96, 96});
    p.wire(p.imageOut(rasterA, 0), p.sinkInput(sinkA));
    p.wire(p.imageOut(rasterB, 0), p.sinkInput(sinkB));

    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = p.error();
      return;
    }
    p.render(6);
    out.filtered = p.readback(sinkA);
    out.unfiltered = p.readback(sinkB);
  });
  return out;
}
}

TEST_CASE(
    "a scene filter's rewritten indirect commands drive its branch only",
    "[gfx][csf][scene-filter][indirect]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool cpuRung = GENERATE(false, true);
  CAPTURE(backend_name(api), cpuRung);

  if(cpuRung)
    qputenv("SCORE_GFX_NO_GPU_INDIRECT", "1");
  else
    qunsetenv("SCORE_GFX_NO_GPU_INDIRECT");
  const Run r = render(api);
  qunsetenv("SCORE_GFX_NO_GPU_INDIRECT");

  if(r.skipped)
    SKIP("backend unavailable: " + r.skip_reason);
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);

  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.filtered.valid());
  REQUIRE(r.unfiltered.valid());

  const int a = litPixels(r.filtered);
  const int b = litPixels(r.unfiltered);
  INFO("filtered branch lit " << a << " px, unfiltered branch lit " << b << " px");
  CHECK(b > 200);
  CHECK(a == 0);
}
