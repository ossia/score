// A raw raster with two outgoing edges binds the geometry's buffers in both.
//
// The raster's uniform and storage INPUTS (camera, scene_counts) are
// name-matched against the ScenePreprocessor geometry's auxiliary buffers.
// Each outgoing edge owns a pass with its own SRB. The first bind replaces the
// placeholder buffers in m_storage and releases them; the SRB of every pass
// must then point at the geometry's buffers. Before the fix only the first
// pass's SRB was patched and the second kept the released placeholders, which
// crashed in QRhiVulkan::setShaderResources once they were deleted.
//
// Registration: see the test_gfx_rawraster_two_edge_geometry_buffers_e3 target.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Threedim/Primitive.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/IsfBindingsBuilder.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderedISFUtils.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>
#include <Gfx/Graph/VertexFallbackPlan.hpp>
#include <ossia/detail/small_flat_map.hpp>

#define private public
#include <Gfx/Graph/RenderedRawRasterPipelineNode.hpp>
#undef private

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

struct Mismatch
{
  std::string name;
  int binding{};
  int pass{};
};

struct Outcome
{
  bool skipped = false;
  std::string error;
  int passes = 0;
  int checkedBindings = 0;
  std::vector<Mismatch> stale;
  ReadbackImage view;
};

QRhiBuffer* boundBuffer(QRhiShaderResourceBindings& srb, int binding)
{
  for(auto it = srb.cbeginBindings(); it != srb.cendBindings(); ++it)
  {
    auto* d = reinterpret_cast<const QRhiShaderResourceBinding::Data*>(&*it);
    if(d->binding != binding)
      continue;
    switch(d->type)
    {
      case QRhiShaderResourceBinding::UniformBuffer:
        return d->u.ubuf.buf;
      case QRhiShaderResourceBinding::BufferLoad:
      case QRhiShaderResourceBinding::BufferStore:
      case QRhiShaderResourceBinding::BufferLoadStore:
        return d->u.sbuf.buf;
      default:
        return nullptr;
    }
  }
  return nullptr;
}

Outcome run(score::gfx::GraphicsApi api)
{
  Outcome out;
  std::vector<std::unique_ptr<Process::ProcessModel>> models;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      out.error = "no document";
      return;
    }
    const auto& ctx = doc->context();
    auto model = std::make_unique<oscr::ProcessModel<Threedim::Cube>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, ctx, nullptr);
    auto* raw = model.get();
    models.push_back(std::move(model));

    GfxPipeline p;
    const int cube = p.addNode(std::unique_ptr<score::gfx::Node>{
        new oscr::GfxNode<Threedim::Cube>{*raw, {}, Gfx::exec_controls{}, 1, ctx}});
    const int flat
        = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int rr = p.addRaster(
        corpus("e3-rr-two-edge-geometry-buffers.vs"),
        corpus("e3-rr-two-edge-geometry-buffers.fs"));
    const int join = p.addIsf(corpus("isf-two-images.fs"));
    if(cube < 0 || flat < 0 || rr < 0 || join < 0)
    {
      out.error = "chain build failed: " + p.error();
      return;
    }
    p.wire(p.nodeSceneOut(cube, 0), p.nodeSceneIn(flat, 0));
    p.wire(p.nodeGeometryOut(flat, 0), p.geometryIn(rr, 0));
    p.wire(p.imageOut(rr, 0), p.imageIn(join, 0));
    p.wire(p.imageOut(rr, 0), p.imageIn(join, 1));
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(join, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    p.render(6);
    out.view = p.readback(sink);

    for(auto& [rl, renderer] : p.isf(rr)->renderedNodes)
    {
      auto* rrp
          = dynamic_cast<score::gfx::RenderedRawRasterPipelineNode*>(renderer);
      if(!rrp)
        continue;
      out.passes = int(rrp->m_passes.size());
      int passIdx = 0;
      for(auto& [edge, pass] : rrp->m_passes)
      {
        if(!pass.p.srb)
          continue;
        auto check = [&](const auto& entries) {
          for(const auto& e : entries)
          {
            if(e.binding < 0 || !e.buffer)
              continue;
            out.checkedBindings++;
            if(boundBuffer(*pass.p.srb, e.binding) != e.buffer)
              out.stale.push_back({e.name, e.binding, passIdx});
          }
        };
        check(rrp->m_storage.ubos);
        check(rrp->m_storage.ssbos);
        passIdx++;
      }
    }
  });
  return out;
}
}

TEST_CASE(
    "every pass of a two-edge raw raster binds the geometry's buffers",
    "[gfx][rawraster][geometry][aux]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Outcome out = run(api);
  if(out.skipped)
    SKIP("no usable RHI backend");
  REQUIRE(out.error.empty());
  REQUIRE(out.passes == 2);
  REQUIRE(out.checkedBindings == 4);
  for(const auto& m : out.stale)
  {
    CAPTURE(m.name, m.binding, m.pass);
    FAIL_CHECK("pass SRB still binds a buffer the node no longer holds");
  }
  CHECK(out.stale.empty());
}
