#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Gfx.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/PreviewHubNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RowOrder.hpp>
#include <Gfx/Settings/Model.hpp>
#include <Gfx/TextureCapture.hpp>
#include <Gfx/Widgets/TexturePreview.hpp>

#include <core/document/Document.hpp>

#include <QCoreApplication>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace
{
constexpr int SrcW = 300, SrcH = 200;

//! Like a camera: its own float texture at its own size.
struct FloatSourceNode final : score::gfx::ProcessNode
{
  enum Publish
  {
    //! Whoever reads it
    Always,
    //! Like the video node: only for a sink that grabs it (outputGrabbedBySink),
    //! otherwise it would draw into the consumer's target
    GrabOnly,
    //! Never: it only draws into its consumers' targets
    Never
  };
  Publish publish{Always};

  FloatSourceNode()
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
  }

  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList&) const noexcept override
  {
    struct Renderer final : score::gfx::NodeRenderer
    {
      using NodeRenderer::NodeRenderer;
      QRhiTexture* m_tex{};
      std::vector<float> m_data;

      void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res) override
      {
        m_tex = r.state.rhi->newTexture(QRhiTexture::RGBA32F, QSize{SrcW, SrcH}, 1);
        m_tex->create();
        m_data.resize(std::size_t(SrcW) * SrcH * 4);
        for(int y = 0; y < SrcH; y++)
          for(int x = 0; x < SrcW; x++)
          {
            float* p = &m_data[(std::size_t(y) * SrcW + x) * 4];
            p[0] = (x + 0.5f) / SrcW;
            p[1] = (SrcH - 1 - y + 0.5f) / SrcH;
            p[2] = 0.123456f;
            p[3] = 1.f;
          }
        const bool reverse = score::gfx::uploadReversesRows(*r.state.rhi);
        std::vector<float> up = m_data;
        if(reverse)
          for(int y = 0; y < SrcH; y++)
            std::memcpy(
                &up[std::size_t(y) * SrcW * 4],
                &m_data[std::size_t(SrcH - 1 - y) * SrcW * 4], SrcW * 4 * sizeof(float));
        res.uploadTexture(
            m_tex, QRhiTextureUploadDescription{{0, 0,
                                                 QRhiTextureSubresourceUploadDescription{
                                                     up.data(), quint32(up.size() * 4)}}});
        m_initialized = true;
      }
      void update(score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*) override
      {
      }
      QRhiTexture* textureForOutput(const score::gfx::Port& p) override
      {
        switch(static_cast<const FloatSourceNode&>(node).publish)
        {
          case Always:
            return m_tex;
          case GrabOnly:
            return score::gfx::outputGrabbedBySink(p) ? m_tex : nullptr;
          default:
            return nullptr;
        }
      }
      void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
      void release(score::gfx::RenderList&) override
      {
        delete m_tex;
        m_tex = nullptr;
      }
    };
    return new Renderer{*this};
  }
};

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

//! Selects `api`, or says why the case is skipped.
bool useBackend(
    const score::GUIApplicationContext& ctx, score::gfx::GraphicsApi api, std::string& skip)
{
  if(api == score::gfx::Null)
  {
    skip = score::test::gfx::null_backend_skip_reason();
    return false;
  }
  std::string probed;
  if(!score::test::gfx::probe_api(api, probed))
  {
    skip = std::string(score::test::gfx::backend_name(api)) + " unavailable";
    return false;
  }
  auto& settings = ctx.settings<Gfx::Settings::Model>();
  settings.setVSync(false);
  settings.setGraphicsApi(apiSettingName(api));
  return true;
}

//! Ticks of the context, each rendering and completing a frame of every output.
void pump(Gfx::GfxContext& g, int frames = 1)
{
  for(int i = 0; i < frames; i++)
  {
    QCoreApplication::processEvents();
    g.updateGraph();
    for(auto* out : g.outputs())
    {
      out->render();
      if(auto rs = out->renderState(); rs && rs->rhi)
        rs->rhi->finish();
    }
  }
}

template <typename F>
bool pumpUntil(Gfx::GfxContext& g, F&& done, int frames = 60)
{
  for(int i = 0; i < frames && !done(); i++)
    pump(g);
  return done();
}

//! The pixels holding exactly what FloatSourceNode uploaded: row 0 is the top
//! (largest green), in float, no 8-bit step.
int exactPixels(const std::vector<float>& frame, int w, int h)
{
  if(w <= 0 || h <= 0 || frame.size() != std::size_t(w) * h * 4)
    return 0;
  int exact = 0;
  for(int y = 0; y < h; y++)
    for(int x = 0; x < w; x++)
    {
      const float* q = &frame[(std::size_t(y) * w + x) * 4];
      exact += std::abs(q[0] - (x + 0.5f) / w) < 1e-5f
               && std::abs(q[1] - (h - 1 - y + 0.5f) / h) < 1e-5f
               && std::abs(q[2] - 0.123456f) < 1e-6f && q[3] == 1.f;
    }
  return exact;
}

struct Frame
{
  std::vector<float> rgba;
  int w{}, h{};
  bool got{};
};

//! Asks for a frame and waits for it.
Frame capture(Gfx::GfxContext& g, Gfx::TextureCapture& cap)
{
  Frame f;
  const int64_t before = cap.frames();
  cap.request();
  if(pumpUntil(g, [&] { return cap.frames() > before; }))
    f.got = cap.take(f.rgba, f.w, f.h);
  return f;
}

//! A live output of the patch, rendering what is cabled into it at its size.
struct LiveSink
{
  score::gfx::BackgroundNode* node{};
  int32_t id{-1};
};

LiveSink addLiveSink(score::Document& doc, Gfx::GfxContext& g, int32_t producer, QSize size)
{
  auto& exec = doc.context().plugin<Gfx::DocumentPlugin>().exec;
  auto owned = std::make_unique<score::gfx::BackgroundNode>();
  owned->shared_readback = std::make_shared<QRhiReadbackResult>();
  owned->setSize(size);
  LiveSink s;
  s.node = owned.get();
  s.id = g.register_node(std::move(owned));
  ossia::audio_tick_state st{};
  exec.startTick(st);
  exec.setEdge(
      Gfx::port_index{producer, 0}, Gfx::port_index{s.id, 0},
      Process::CableType::ImmediateGlutton);
  exec.endTick(st);
  g.updateGraph();
  return s;
}
}

TEST_CASE(
    "a texture capture reads the producer back at its own size, in float, top row "
    "first, with the score stopped, one frame per request",
    "[gfx][preview][capture][gui]")
{
  const auto api = GENERATE(from_range(score::test::gfx::platform_backends()));
  const auto publish = GENERATE(FloatSourceNode::Always, FloatSourceNode::GrabOnly);
  CAPTURE(int(publish));
  CAPTURE(score::test::gfx::backend_name(api));

  std::string skip;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    if(!useBackend(ctx, api, skip))
      return;
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& g = doc->context().plugin<Gfx::DocumentPlugin>().context;

    auto src = std::make_unique<FloatSourceNode>();
    src->publish = publish;
    const int32_t p = g.register_node(std::move(src));
    {
      Gfx::TexturePreview tp;
      if(publish == FloatSourceNode::Always)
        tp.attach(&g, Gfx::port_index{p, 0}, QSize{32, 32});
      Gfx::TextureCapture cap{g, Gfx::port_index{p, 0}};

      REQUIRE(pumpUntil(
          g, [&] { return cap.state() == Gfx::TextureCapture::State::Capturing; }));
      pump(g, 4);
      CHECK(cap.frames() == 0);

      const Frame f = capture(g, cap);
      REQUIRE(f.got);
      CHECK(f.w == SrcW);
      CHECK(f.h == SrcH);
      CHECK(exactPixels(f.rgba, f.w, f.h) == SrcW * SrcH);
      CHECK(cap.frames() == 1);

      pump(g, 6);
      CHECK(cap.frames() == 1);
      std::vector<float> none;
      int w{}, h{};
      CHECK(!cap.take(none, w, h));

      CHECK(capture(g, cap).got);
      pump(g, 4);
      CHECK(cap.frames() == 2);

      if(publish == FloatSourceNode::Always)
      {
        pumpUntil(g, [&] { return !tp.image().isNull(); });
        CHECK(!tp.image().isNull());
        CHECK(tp.image().width() <= 32);
      }
    }
    g.unregister_node(p);
    g.updateGraph();
  });
  if(!skip.empty())
    SKIP(skip);
}

TEST_CASE(
    "a texture capture of a producer a live output renders into a smaller target "
    "reads the producer's own texture",
    "[gfx][preview][capture][gui]")
{
  const auto api = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(api));

  std::string skip;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    if(!useBackend(ctx, api, skip))
      return;
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& g = doc->context().plugin<Gfx::DocumentPlugin>().context;

    auto src = std::make_unique<FloatSourceNode>();
    src->publish = FloatSourceNode::GrabOnly;
    const int32_t p = g.register_node(std::move(src));
    const LiveSink sink = addLiveSink(*doc, g, p, QSize{64, 48});
    pump(g, 3);
    if(!sink.node->renderer())
    {
      skip = "the live output did not come up";
      return;
    }
    const auto outputsBefore = g.outputs().size();
    {
      Gfx::TextureCapture cap{g, Gfx::port_index{p, 0}};
      REQUIRE(pumpUntil(
          g, [&] { return cap.state() == Gfx::TextureCapture::State::Capturing; }));
      CHECK(g.outputs().size() == outputsBefore);

      const Frame f = capture(g, cap);
      REQUIRE(f.got);
      CHECK(f.w == SrcW);
      CHECK(f.h == SrcH);
      CHECK(exactPixels(f.rgba, f.w, f.h) == SrcW * SrcH);
    }
    g.unregister_node(sink.id);
    g.unregister_node(p);
    g.updateGraph();
  });
  if(!skip.empty())
    SKIP(skip);
}

TEST_CASE(
    "a texture capture never reads the target a live output renders a producer into",
    "[gfx][preview][capture][gui]")
{
  const auto api = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(api));

  std::string skip;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    if(!useBackend(ctx, api, skip))
      return;
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& g = doc->context().plugin<Gfx::DocumentPlugin>().context;

    auto src = std::make_unique<FloatSourceNode>();
    src->publish = FloatSourceNode::Never;
    const int32_t p = g.register_node(std::move(src));
    const LiveSink sink = addLiveSink(*doc, g, p, QSize{64, 48});
    pump(g, 3);
    if(!sink.node->renderer())
    {
      skip = "the live output did not come up";
      return;
    }
    const auto outputsBefore = g.outputs().size();
    {
      Gfx::TextureCapture cap{g, Gfx::port_index{p, 0}};
      cap.request();
      pump(g, 8);
      CHECK(cap.frames() == 0);
      CHECK(cap.state() == Gfx::TextureCapture::State::Waiting);
      CHECK(!cap.reason().isEmpty());
      CHECK(g.outputs().size() == outputsBefore);
    }
    g.unregister_node(sink.id);
    g.unregister_node(p);
    g.updateGraph();
  });
  if(!skip.empty())
    SKIP(skip);
}

TEST_CASE(
    "a texture capture that cannot read the producer's own texture says so, and "
    "never reads a resampled image",
    "[gfx][preview][capture][gui]")
{
  const auto api = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(api));

  std::string skip;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    if(!useBackend(ctx, api, skip))
      return;
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& g = doc->context().plugin<Gfx::DocumentPlugin>().context;
    using State = Gfx::TextureCapture::State;

    const auto outputsBefore = g.outputs().size();

    SECTION("a producer with no texture of its own")
    {
      auto src = std::make_unique<FloatSourceNode>();
      src->publish = FloatSourceNode::Never;
      const int32_t p = g.register_node(std::move(src));
      {
        Gfx::TextureCapture cap{g, Gfx::port_index{p, 0}};
        cap.request();
        pump(g, 8);
        CHECK(cap.state() == State::Waiting);
        CHECK(!cap.reason().isEmpty());
        CHECK(cap.frames() == 0);
        CHECK(g.outputs().size() <= outputsBefore + 1);
      }
      g.unregister_node(p);
      g.updateGraph();
    }

    SECTION("no producer, then one")
    {
      Gfx::TextureCapture cap{g, Gfx::port_index{987654, 0}};
      pump(g, 2);
      CHECK(cap.state() == State::Unavailable);
      CHECK(!cap.reason().isEmpty());

      auto src = std::make_unique<FloatSourceNode>();
      const int32_t p = g.register_node(std::move(src));
      cap.setProducer(Gfx::port_index{p, 0});
      REQUIRE(pumpUntil(g, [&] { return cap.state() == State::Capturing; }));
      const Frame f = capture(g, cap);
      CHECK(f.got);
      CHECK(exactPixels(f.rgba, f.w, f.h) == SrcW * SrcH);
      g.unregister_node(p);
      g.updateGraph();
    }

    SECTION("more captures than the hub has inputs for")
    {
      auto src = std::make_unique<FloatSourceNode>();
      const int32_t p = g.register_node(std::move(src));
      {
        std::vector<std::unique_ptr<Gfx::TextureCapture>> caps;
        for(int i = 0; i < score::gfx::PreviewHubNode::capture_inputs + 1; i++)
          caps.push_back(std::make_unique<Gfx::TextureCapture>(g, Gfx::port_index{p, 0}));
        pump(g, 2);
        for(int i = 0; i < score::gfx::PreviewHubNode::capture_inputs; i++)
        {
          CAPTURE(i);
          CHECK(caps[i]->state() == State::Capturing);
        }
        CHECK(caps.back()->state() == State::Unavailable);
        CHECK(!caps.back()->reason().isEmpty());

        const Frame a = capture(g, *caps[0]);
        const Frame b = capture(g, *caps[1]);
        CHECK(exactPixels(a.rgba, a.w, a.h) == SrcW * SrcH);
        CHECK(exactPixels(b.rgba, b.w, b.h) == SrcW * SrcH);

        caps.erase(caps.begin());
        REQUIRE(pumpUntil(g, [&] { return caps.back()->state() == State::Capturing; }));
        CHECK(capture(g, *caps.back()).got);
      }
      g.unregister_node(p);
      g.updateGraph();
    }
  });
  if(!skip.empty())
    SKIP(skip);
}

TEST_CASE(
    "a texture capture or its context destroyed while a readback is pending",
    "[gfx][preview][capture][gui]")
{
  const auto api = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(api));

  std::string skip;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    if(!useBackend(ctx, api, skip))
      return;
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& g = doc->context().plugin<Gfx::DocumentPlugin>().context;
    using State = Gfx::TextureCapture::State;

    auto src = std::make_unique<FloatSourceNode>();
    const int32_t p = g.register_node(std::move(src));

    SECTION("the capture, before the frame renders")
    {
      auto cap = std::make_unique<Gfx::TextureCapture>(g, Gfx::port_index{p, 0});
      REQUIRE(pumpUntil(g, [&] { return cap->state() == State::Capturing; }));
      cap->request();
      cap.reset();
      pump(g, 3);
      g.unregister_node(p);
      g.updateGraph();
      pump(g, 2);
    }

    SECTION("the document")
    {
      auto cap = std::make_unique<Gfx::TextureCapture>(g, Gfx::port_index{p, 0});
      REQUIRE(pumpUntil(g, [&] { return cap->state() == State::Capturing; }));
      cap->request();
      ctx.docManager.forceCloseDocument(ctx, *doc);
      for(int i = 0; i < 5; i++)
        QCoreApplication::processEvents();
      CHECK(cap->state() == State::Unavailable);
      CHECK(!cap->reason().isEmpty());
      std::vector<float> rgba;
      int w{}, h{};
      CHECK(!cap->take(rgba, w, h));
      cap->request();
      cap->setProducer(Gfx::port_index{p, 0});
      cap.reset();
    }
  });
  if(!skip.empty())
    SKIP(skip);
}
