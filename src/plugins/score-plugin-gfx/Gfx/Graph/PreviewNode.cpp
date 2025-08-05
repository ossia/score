#include "PreviewNode.hpp"

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

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
#ifndef QT_NO_OPENGL
      state.version = score::GLCapabilities{}.qShaderVersion;
#endif
      break;
    case QRhi::Vulkan:
      state.api = score::gfx::Vulkan;
      state.version = QShaderVersion(100);
      break;
    case QRhi::Metal:
      state.api = score::gfx::Metal;
      state.version = QShaderVersion(12);
      break;
    case QRhi::D3D11:
      state.api = score::gfx::D3D11;
      state.version = QShaderVersion(50);
      break;
    case QRhi::D3D12:
      state.api = score::gfx::D3D12;
      state.version = QShaderVersion(50);
      break;
    case QRhi::Null:
      state.api = score::gfx::Null;
      state.version = QShaderVersion(120);
      break;
  }

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
  if(m_update)
    m_update();

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

void PreviewNode::createOutput(
    score::gfx::GraphicsApi graphicsApi, std::function<void()> onReady,
    std::function<void()> onUpdate, std::function<void()> onResize)
{
  m_renderState = std::make_shared<score::gfx::RenderState>();
  m_update = onUpdate;

  m_renderState = importRenderState(QSize(m_settings.width, m_settings.height), m_rhi);
  m_renderState->renderPassDescriptor = m_renderTarget->renderPassDescriptor();

  onReady();
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

score::gfx::OutputNodeRenderer*
PreviewNode::createRenderer(score::gfx::RenderList& r) const noexcept
{
  score::gfx::TextureRenderTarget rt{
      m_texture, nullptr, nullptr, m_renderState->renderPassDescriptor, m_renderTarget};
  return new score::gfx::PreviewRenderer{*this, rt};
}

}
