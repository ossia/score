#include "PreviewNode.hpp"

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/gfx/OpenGL.hpp>
namespace score::gfx
{
std::shared_ptr<RenderState> importRenderState(QSize sz, QRhi* rhi)
{
  auto st = std::make_shared<RenderState>();
  RenderState& state = *st;
  switch(rhi->backend())
  {
    case QRhi::OpenGLES2:
      state.api = score::gfx::OpenGL;
      break;
    case QRhi::Vulkan:
      state.api = score::gfx::Vulkan;
      break;
    case QRhi::Metal:
      state.api = score::gfx::Metal;
      break;
    case QRhi::D3D11:
      state.api = score::gfx::D3D11;
      break;
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    case QRhi::D3D12:
      state.api = score::gfx::D3D12;
      break;
#endif
    case QRhi::Null:
      state.api = score::gfx::Null;
      break;
  }
  state.version = Gfx::Settings::shaderVersionForAPI(state.api);
  state.rhi = rhi;
  state.samples = 1; // FIXME
  state.renderSize = sz;
  state.outputSize = sz;
  return st;
}

PreviewNode::PreviewNode(
    Gfx::SharedOutputSettings s, QRhi* rhi, QRhiRenderTarget* tgt, QRhiTexture* tex)
    : OutputNode{}
    , m_settings{std::move(s)}
    , m_rhi{rhi}
    , m_renderTarget{tgt}
    , m_texture{tex}
{
  input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
}

PreviewNode::~PreviewNode() { }

bool PreviewNode::canRender() const
{
  return true;
}

void PreviewNode::startRendering() { }

void PreviewNode::render()
{
  auto renderer = m_renderer.lock();
  if(renderer && m_renderState)
  {
    auto rhi = m_renderState->rhi;
    QRhiCommandBuffer* cb{};
    if(rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
      return;

    renderer->render(*cb);
    rhi->endOffscreenFrame();
  }
}

score::gfx::OutputNode::Configuration PreviewNode::configuration() const noexcept
{
  return {};
}

void PreviewNode::onRendererChange() { }

void PreviewNode::stopRendering() { }

void PreviewNode::setRenderer(std::shared_ptr<score::gfx::RenderList> r)
{
  m_renderer = r;
}

score::gfx::RenderList* PreviewNode::renderer() const
{
  return m_renderer.lock().get();
}

void PreviewNode::createOutput(score::gfx::OutputConfiguration conf)
{
  m_renderState = std::make_shared<score::gfx::RenderState>();

  m_renderState = importRenderState(QSize(m_settings.width, m_settings.height), m_rhi);
  m_renderState->renderPassDescriptor = m_renderTarget->renderPassDescriptor();

  conf.onReady();
}

void PreviewNode::destroyOutput() { }

std::shared_ptr<score::gfx::RenderState> PreviewNode::renderState() const
{
  return m_renderState;
}

class PreviewRenderer final : public score::gfx::OutputNodeRenderer
{
public:
  explicit PreviewRenderer(const score::gfx::Node& n, score::gfx::TextureRenderTarget rt)
      : score::gfx::OutputNodeRenderer{n}
      , m_inputTarget{std::move(rt)}
  {
  }

  score::gfx::TextureRenderTarget m_inputTarget;
  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    return m_inputTarget;
  }

  void finishFrame(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch*& res) override
  {
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override { }
  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* edge) override
  {
  }
  void release(score::gfx::RenderList&) override { }
};

class PreviewRendererInvertY final : public score::gfx::OutputNodeRenderer
{
  score::gfx::TextureRenderTarget m_inputTarget;
  score::gfx::TextureRenderTarget m_renderTarget;

  QShader m_vertexS, m_fragmentS;

  std::vector<score::gfx::Sampler> m_samplers;

  score::gfx::Pipeline m_p;

  score::gfx::MeshBuffers m_mesh{};

public:
  explicit PreviewRendererInvertY(
      const score::gfx::Node& n, score::gfx::TextureRenderTarget rt)
      : score::gfx::OutputNodeRenderer{n}
      , m_renderTarget{std::move(rt)}
  {
  }

  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    return m_inputTarget;
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    m_inputTarget = score::gfx::createRenderTarget(
        renderer.state, QRhiTexture::Format::RGBA8, m_renderTarget.texture->pixelSize(),
        renderer.samples(), renderer.requiresDepth(*this->node.input[0]));

    const auto& mesh = renderer.defaultTriangle();
    m_mesh = renderer.initMeshBuffer(mesh, res);

    // Note that unlike the "InvertYRenderer", here we leverage the
    // coordinate inversion inherent to metal rendering (e.g. y direction is
    // different in viewport vs texture, thus displaying a texture "inverts" it)
    static const constexpr auto gl_filter = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;

    layout(binding = 3) uniform sampler2D tex;

    void main()
    {
      fragColor = texture(tex, vec2(v_texcoord.x,  v_texcoord.y));
    }
    )_";
    std::tie(m_vertexS, m_fragmentS)
        = score::gfx::makeShaders(renderer.state, mesh.defaultVertexShader(), gl_filter);

    // Put the input texture, where all the input nodes are rendering, in a sampler.
    {
      auto sampler = renderer.state.rhi->newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);

      sampler->setName("PreviewRendererInvertY::sampler");
      sampler->create();

      m_samplers.push_back({sampler, this->m_inputTarget.texture});
    }

    m_p = score::gfx::buildPipeline(
        renderer, mesh, m_vertexS, m_fragmentS, m_renderTarget, nullptr, nullptr,
        m_samplers);
  }
  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* edge) override
  {
  }
  void release(score::gfx::RenderList&) override
  {
    m_p.release();
    for(auto& s : m_samplers)
    {
      delete s.sampler;
    }
    m_samplers.clear();
    m_inputTarget.release();
  }

  void finishFrame(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch*& res) override
  {
    cb.beginPass(m_renderTarget.renderTarget, Qt::black, {1.0f, 0}, res);
    res = nullptr;
    {
      const auto sz = renderer.state.renderSize;
      cb.setGraphicsPipeline(m_p.pipeline);
      cb.setShaderResources(m_p.srb);
      cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));

      const auto& mesh = renderer.defaultTriangle();
      mesh.draw(this->m_mesh, cb);
    }
    cb.endPass(nullptr);
  }
};

score::gfx::OutputNodeRenderer*
PreviewNode::createRenderer(score::gfx::RenderList& r) const noexcept
{
  score::gfx::TextureRenderTarget rt{
      m_texture, nullptr, nullptr, m_renderState->renderPassDescriptor, m_renderTarget};
  switch(r.state.api)
  {
    default:
    case score::gfx::GraphicsApi::OpenGL:
    case score::gfx::GraphicsApi::Vulkan:
      return new score::gfx::PreviewRenderer{*this, rt};
      break;
    case score::gfx::GraphicsApi::Metal:
    case score::gfx::GraphicsApi::D3D11:
    case score::gfx::GraphicsApi::D3D12:
      return new score::gfx::PreviewRendererInvertY{*this, rt};
      break;
  }
  return nullptr;
}

}
