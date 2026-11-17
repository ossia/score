#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Gfx.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RowOrder.hpp>

#include <cstring>
#include <Gfx/Settings/Model.hpp>
#include <Gfx/TextureCapture.hpp>
#include <Gfx/Widgets/TexturePreview.hpp>

#include <core/document/Document.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <chrono>
#include <cmath>
#include <string>
#include <thread>

namespace
{
constexpr int SrcW = 300, SrcH = 200;

//! Like a camera: its own float texture at its own size, whoever reads it.
struct FloatSourceNode final : score::gfx::ProcessNode
{
  //! Like the video node: its own texture only for a sink that grabs it,
  //! otherwise it would draw into the consumer's target.
  bool grabOnly{};
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
        const bool grabOnly = static_cast<const FloatSourceNode&>(node).grabOnly;
        return !grabOnly || score::gfx::outputGrabbedBySink(p) ? m_tex : nullptr;
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
}

TEST_CASE(
    "a texture capture reads the producer back at its own size, in float, top row "
    "first, with the score stopped",
    "[gfx][preview][capture][gui]")
{
  const auto api = GENERATE(from_range(score::test::gfx::platform_backends()));
  const bool grabOnly = GENERATE(false, true);
  CAPTURE(grabOnly);
  CAPTURE(score::test::gfx::backend_name(api));

  std::string skip;
  bool got = false;
  int w{}, h{};
  std::vector<float> frame;
  QSize thumb;
  int64_t framesBefore{}, framesAfter{};

  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    if(api == score::gfx::Null)
    {
      skip = score::test::gfx::null_backend_skip_reason();
      return;
    }
    std::string probed;
    if(!score::test::gfx::probe_api(api, probed))
    {
      skip = std::string(score::test::gfx::backend_name(api)) + " unavailable";
      return;
    }
    auto& settings = ctx.settings<Gfx::Settings::Model>();
    settings.setVSync(false);
    settings.setGraphicsApi(apiSettingName(api));

    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& g = doc->context().plugin<Gfx::DocumentPlugin>().context;

    auto src = std::make_unique<FloatSourceNode>();
    src->grabOnly = grabOnly;
    const int32_t p = g.register_node(std::move(src));

    const auto renderAll = [&] {
      for(int i = 0; i < 4; i++)
      {
        g.updateGraph();
        for(auto* out : g.outputs())
        {
          out->render();
          if(auto rs = out->renderState(); rs && rs->rhi)
            rs->rhi->finish();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
      }
    };

    {
      Gfx::TexturePreview tp;
      if(!grabOnly)
        tp.attach(&g, Gfx::port_index{p, 0}, QSize{32, 32});
      Gfx::TextureCapture cap{g, Gfx::port_index{p, 0}};
      for(int i = 0; i < 10 && !got; i++)
      {
        renderAll();
        got = cap.take(frame, w, h);
      }
      thumb = tp.image().size();
      framesBefore = cap.frames();
      renderAll();
      renderAll();
      framesAfter = cap.frames();
    }

    g.unregister_node(p);
    g.updateGraph();
  });

  if(!skip.empty())
    SKIP(skip);
  REQUIRE(got);
  INFO(w << "x" << h << ", thumbnail " << thumb.width() << "x" << thumb.height());
  if(!grabOnly)
    CHECK(thumb.width() <= 32);
  CHECK(w == SrcW);
  CHECK(h == SrcH);
  REQUIRE(frame.size() == std::size_t(w) * h * 4);
  int exact = 0;
  for(int y = 0; y < h; y++)
    for(int x = 0; x < w; x += 37)
    {
      const float* q = &frame[(std::size_t(y) * w + x) * 4];
      exact += std::abs(q[0] - (x + 0.5f) / w) < 1e-5f
               && std::abs(q[1] - (h - 1 - y + 0.5f) / h) < 1e-5f
               && std::abs(q[2] - 0.123456f) < 1e-6f;
    }
  // Row 0 is the top (largest green), in float: no 8-bit step anywhere.
  CHECK(exact == h * ((w + 36) / 37));
  // Taken, so the next frame is read back; not taken, none more.
  CHECK(framesAfter - framesBefore <= 1);
}
