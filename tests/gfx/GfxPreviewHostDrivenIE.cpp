// A QML TextureSource preview is rendered once per frame, by its host only.
//
// TextureSource registers a score::gfx::PreviewNode in the document's
// GfxContext and renders that output's RenderList itself, from Qt Quick's
// frame (TextureSourceRenderer::render). GfxContext::renderFrames and the
// free-wheel tick used to call render() on every output as well, so the
// preview's RenderList -- and every upstream node in it -- ran twice per
// frame. PreviewNode now declares itself host-driven.
//
// One ISF producer feeds an offscreen sink and a PreviewNode whose host texture
// lives on the sink's QRhi. Per frame, the test runs renderFrames(1) and then
// the host's own render of the preview's list, and counts the preview list's
// frames and the PreviewNode::render() calls. Asserted: renderFrames never
// renders the preview, the host render does, so the preview list advances by
// exactly one per frame, while the offscreen sink keeps advancing by one.
//
// Registration: see test_gfx_preview_host_driven_ie.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Gfx.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/PreviewNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Settings/Model.hpp>

#include <core/document/Document.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <memory>

namespace
{
QString corpus(const char* name)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(name);
}

QString apiSettingName(score::gfx::GraphicsApi api)
{
  const Gfx::Settings::GraphicsApis apis{};
  switch(api)
  {
    case score::gfx::Vulkan:
      return apis.Vulkan;
    case score::gfx::Metal:
      return apis.Metal;
    case score::gfx::D3D11:
      return apis.D3D11;
    case score::gfx::D3D12:
      return apis.D3D12;
    default:
      return apis.OpenGL;
  }
}

struct CountingPreview final : score::gfx::PreviewNode
{
  using score::gfx::PreviewNode::PreviewNode;
  int renders{};
  void render() override
  {
    ++renders;
    score::gfx::PreviewNode::render();
  }
};

struct Outcome
{
  std::string skip;
  bool built{};
  int frames{};
  int64_t previewFramesFromRenderFrames{-1};
  int64_t previewFramesFromHost{-1};
  int previewRenderCalls{-1};
  int64_t sinkFrames{-1};
};

Outcome run(score::gfx::GraphicsApi backend)
{
  Outcome out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto& settings = app.settings<Gfx::Settings::Model>();
    settings.setVSync(false);
    settings.setRate(60.);
    settings.setGraphicsApi(apiSettingName(backend));

    std::string probed;
    if(!score::test::gfx::probe_api(backend, probed))
    {
      out.skip = std::string("RHI backend '") + score::test::gfx::backend_name(backend)
                 + "' unavailable";
      return;
    }

    score::Document* doc = score::test::new_document(app);
    REQUIRE(doc);
    auto& plug = doc->context().plugin<Gfx::DocumentPlugin>();
    auto& g = plug.context;
    auto& exec = plug.exec;

    auto isf = score::test::gfx::make_isf_node(corpus("isf-solid-color.fs"));
    REQUIRE(isf.node);
    auto sinkOwned = std::make_unique<score::gfx::BackgroundNode>();
    sinkOwned->shared_readback = std::make_shared<QRhiReadbackResult>();
    auto* sink = sinkOwned.get();

    const int32_t a = g.register_node(std::move(isf.node));
    const int32_t s = g.register_node(std::move(sinkOwned));
    {
      ossia::audio_tick_state st{};
      exec.startTick(st);
      exec.setEdge(
          Gfx::port_index{a, 0}, Gfx::port_index{s, 0},
          Process::CableType::ImmediateGlutton);
      exec.endTick(st);
    }
    g.updateGraph();
    g.renderFrames(2);

    auto* sinkRl = sink->renderer();
    if(!sinkRl || !sink->renderState() || !sink->renderState()->rhi)
      return;
    QRhi* rhi = sink->renderState()->rhi;

    std::unique_ptr<QRhiTexture> tex{
        rhi->newTexture(QRhiTexture::RGBA8, QSize{32, 32}, 1, QRhiTexture::RenderTarget)};
    REQUIRE(tex->create());
    std::unique_ptr<QRhiTextureRenderTarget> rt{
        rhi->newTextureRenderTarget({QRhiColorAttachment{tex.get()}})};
    std::unique_ptr<QRhiRenderPassDescriptor> rp{
        rt->newCompatibleRenderPassDescriptor()};
    rt->setRenderPassDescriptor(rp.get());
    REQUIRE(rt->create());

    Gfx::SharedOutputSettings set;
    set.width = 32;
    set.height = 32;
    set.rate = 60;
    auto previewOwned
        = std::make_unique<CountingPreview>(set, rhi, rt.get(), tex.get());
    auto* preview = previewOwned.get();
    const int32_t pv = g.register_preview_node(std::move(previewOwned));
    g.connect_preview_node(Gfx::EdgeSpec{{a, 0}, {pv, 0}});
    g.updateGraph();

    auto* previewRl = preview->renderer();
    if(!previewRl || previewRl->renderers.size() < 2)
    {
      g.unregister_preview_node(pv);
      g.updateGraph();
      return;
    }
    out.built = true;

    preview->renders = 0;
    int64_t fromRenderFrames = 0;
    int64_t fromHost = 0;
    const int64_t sink0 = sinkRl->frame;
    constexpr int frames = 5;
    for(int i = 0; i < frames; i++)
    {
      int64_t before = previewRl->frame;
      g.renderFrames(1);
      fromRenderFrames += previewRl->frame - before;

      before = previewRl->frame;
      QRhiCommandBuffer* cb{};
      if(rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess)
      {
        previewRl->render(*cb);
        rhi->endOffscreenFrame();
      }
      fromHost += previewRl->frame - before;
    }
    out.frames = frames;
    out.previewFramesFromRenderFrames = fromRenderFrames;
    out.previewFramesFromHost = fromHost;
    out.previewRenderCalls = preview->renders;
    out.sinkFrames = sinkRl->frame - sink0;

    g.unregister_preview_node(pv);
    g.updateGraph();
    g.unregister_node(s);
    g.unregister_node(a);
    g.updateGraph();
  });
  return out;
}
}

TEST_CASE(
    "a host-driven preview's render list runs once per frame",
    "[gfx][preview][render][ie]")
{
  const auto backend = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(backend));

  const Outcome o = run(backend);
  if(!o.skip.empty())
    SKIP(o.skip);
  REQUIRE(o.built);

  CHECK(o.previewRenderCalls == 0);
  CHECK(o.previewFramesFromRenderFrames == 0);
  CHECK(o.previewFramesFromHost == o.frames);
  CHECK(o.previewFramesFromRenderFrames + o.previewFramesFromHost == o.frames);
  CHECK(o.sinkFrames == o.frames);
}
