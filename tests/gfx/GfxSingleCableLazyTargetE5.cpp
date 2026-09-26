// A single-cable texture inlet only holds a render target while it needs one.
//
// Texture Info's inlet (halp_flag(single_cable)) reads the upstream's own
// texture when the upstream publishes one. In that case nothing is composited
// into the inlet, so it has no render target: neither the node's own
// (renderTargetForInput) nor the render list's (renderTargetForInputPort).
// Fed by a node that renders into its consumer (a simple ISF) it gets one,
// the upstream draws into it, and it holds the upstream's colour (magenta).
// The cable is swapped at runtime csf -> isf -> csf through the incremental
// edge path: the target appears for the isf and is released again after.
//
// Registration: see test_gfx_single_cable_lazy_target_e5.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <Gfx/Graph/RenderList.hpp>

#include <Threedim/TextureInfo.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
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

struct Producer
{
  const char* path;
  bool compute;
};

const Producer csf37{"sc-csf-image-37x23-rgba32f.cs", true};
const Producer isfSolid{"isf-solid-color.fs", false};

struct Step
{
  bool nodeTarget{};
  bool listTarget{};
  bool readsUpstream{};
  bool readsRenderTarget{};
  bool composited{};
  std::array<int, 4> centre{-1, -1, -1, -1};
};

struct Run
{
  bool skipped{};
  std::string error;
  std::vector<Step> steps;
};

std::array<int, 4> readCentre(QRhi& rhi, QRhiTexture* tex)
{
  std::array<int, 4> px{-1, -1, -1, -1};
  QRhiCommandBuffer* cb{};
  if(rhi.beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
    return px;
  QRhiReadbackResult rr;
  auto* batch = rhi.nextResourceUpdateBatch();
  batch->readBackTexture(QRhiReadbackDescription{tex}, &rr);
  cb->resourceUpdate(batch);
  rhi.endOffscreenFrame();
  const int w = rr.pixelSize.width();
  const int h = rr.pixelSize.height();
  if(w <= 0 || h <= 0 || rr.data.size() < w * h * 4)
    return px;
  const auto* d = reinterpret_cast<const unsigned char*>(rr.data.constData())
                  + (std::size_t(h / 2) * w + w / 2) * 4;
  for(int i = 0; i < 4; ++i)
    px[i] = d[i];
  return px;
}

Run run(score::gfx::GraphicsApi api, std::vector<Producer> producers)
{
  Run out;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      out.error = "no document";
      return;
    }
    const auto& ctx = doc->context();
    GfxPipeline p;

    std::vector<int> prods;
    for(auto& pr : producers)
    {
      const int idx = pr.compute ? p.addCsf(corpus(pr.path)) : p.addIsf(corpus(pr.path));
      if(idx < 0)
      {
        out.error = "producer build failed: " + p.error();
        return;
      }
      prods.push_back(idx);
    }

    auto model = std::make_unique<oscr::ProcessModel<Threedim::TextureInfo>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, ctx, nullptr);
    std::unique_ptr<score::gfx::Node> infoOwned{
        new oscr::GfxNode<Threedim::TextureInfo>{
            *model, {}, Gfx::exec_controls{}, 1, ctx}};
    auto* infoNode = infoOwned.get();
    auto* info = static_cast<score::gfx::OutputNode*>(infoOwned.get());
    const int ti = p.addNode(std::move(infoOwned));
    score::gfx::Port* in{};
    for(auto* port : p.node(ti)->input)
      if(port->type == score::gfx::Types::Image)
      {
        in = port;
        break;
      }
    REQUIRE(in);
    p.wire(p.imageOut(prods[0], 0), in);

    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }

    for(std::size_t k = 0; k < prods.size(); ++k)
    {
      const int prod = prods[k];
      if(k > 0)
      {
        p.removeEdgeIncremental(p.imageOut(prods[k - 1], 0), in);
        p.addEdgeIncremental(p.imageOut(prod, 0), in);
      }
      for(int f = 0; f < 4; ++f)
      {
        p.render(1);
        info->render();
      }

      REQUIRE(!infoNode->renderedNodes.empty());
      auto* rn = dynamic_cast<oscr::GfxRenderer<Threedim::TextureInfo>*>(
          infoNode->renderedNodes.begin()->second);
      REQUIRE(rn);
      REQUIRE(in->edges.size() == 1);
      auto* rl = info->renderer();
      REQUIRE(rl);

      Step step;
      const auto& t = rn->state->inputs.texture.texture;
      auto* upstream = p.isf(prod)->renderedNodes.begin()->second;
      auto* upTex = upstream->textureForOutput(*p.imageOut(prod, 0));
      step.readsUpstream = upTex && t.handle == upTex;
      auto* rt = rn->renderTargetForInput(*in).texture;
      step.nodeTarget = rt != nullptr;
      step.listTarget = rl->renderTargetForInputPort(*in).texture != nullptr;
      step.readsRenderTarget = rt && t.handle == rt;
      step.composited = upstream->hasOutputPassForEdge(*in->edges.front());
      if(rt && rl->state.rhi)
        step.centre = readCentre(*rl->state.rhi, rt);
      out.steps.push_back(step);
    }
  });
  return out;
}

void checkDirect(const Step& s)
{
  CHECK(s.readsUpstream);
  CHECK(!s.composited);
  CHECK(!s.nodeTarget);
  CHECK(!s.listTarget);
}

void checkFallback(const Step& s)
{
  CHECK(s.nodeTarget);
  CHECK(s.readsRenderTarget);
  CHECK(s.composited);
  INFO(
      "centre " << s.centre[0] << "," << s.centre[1] << "," << s.centre[2] << ","
                << s.centre[3]);
  CHECK(s.centre[0] > 200);
  CHECK(s.centre[1] < 50);
  CHECK(s.centre[2] > 200);
}
}

TEST_CASE(
    "A single-cable inlet has no render target while it reads its upstream directly",
    "[gfx][avnd][texture][single-cable][e5]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Run direct = run(api, {csf37});
  if(direct.skipped)
    SKIP("backend unavailable");
  INFO("error=" << direct.error);
  REQUIRE(direct.error.empty());
  REQUIRE(direct.steps.size() == 1);
  checkDirect(direct.steps[0]);

  const Run fallback = run(api, {isfSolid});
  INFO("error=" << fallback.error);
  REQUIRE(fallback.error.empty());
  REQUIRE(fallback.steps.size() == 1);
  checkFallback(fallback.steps[0]);
}

TEST_CASE(
    "A single-cable inlet allocates and releases its render target as the cable changes",
    "[gfx][avnd][texture][single-cable][e5]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Run r = run(api, {csf37, isfSolid, csf37});
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.steps.size() == 3);
  {
    INFO("step 0");
    checkDirect(r.steps[0]);
  }
  {
    INFO("step 1");
    checkFallback(r.steps[1]);
  }
  {
    INFO("step 2");
    checkDirect(r.steps[2]);
  }
}
