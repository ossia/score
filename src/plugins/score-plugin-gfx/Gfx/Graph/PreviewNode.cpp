#include "PreviewNode.hpp"

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <score/gfx/OpenGL.hpp>
namespace score::gfx
{

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

  m_renderState->surface = QRhiGles2InitParams::newFallbackSurface();
  QRhiGles2InitParams params;
  params.fallbackSurface = m_renderState->surface;
  score::GLCapabilities caps;
  caps.setupFormat(params.format);
  // m_rhi = QRhi::create(QRhi::OpenGLES2, &params, {});

  m_renderState->rhi = m_rhi;
  m_renderState->renderSize = QSize(m_settings.width, m_settings.height);
  m_renderState->outputSize = m_renderState->renderSize;
  m_renderState->api = score::gfx::GraphicsApi::OpenGL;
  m_renderState->version = caps.qShaderVersion; // FIXME works only for GL

  m_renderState->renderPassDescriptor = m_renderTarget->renderPassDescriptor();

  qDebug(Q_FUNC_INFO);
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
