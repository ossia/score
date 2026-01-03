#include "LibavEncoderNode.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/Libav/LibavEncoder.hpp>

#include <score/gfx/OpenGL.hpp>
#include <score/gfx/QRhiGles2.hpp>

#include <QOffscreenSurface>

namespace Gfx
{

LibavEncoderNode::LibavEncoderNode(
    const LibavOutputSettings& set, LibavEncoder& encoder, int stream)
    : OutputNode{}
    , encoder{encoder}
    , stream{stream}
    , m_settings{set}
{
  input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
}

LibavEncoderNode::~LibavEncoderNode() { }
bool LibavEncoderNode::canRender() const
{
  return true;
}

void LibavEncoderNode::startRendering() { }

void LibavEncoderNode::render()
{
  if(!encoder.available())
    return;

  auto renderer = m_renderer.lock();
  if(renderer && m_renderState)
  {
    auto rhi = m_renderState->rhi;
    QRhiCommandBuffer* cb{};
    if(rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
      return;

    renderer->render(*cb);
    rhi->endOffscreenFrame();

    int sz = m_readback.pixelSize.width() * m_readback.pixelSize.height() * 4;
    int bytes = m_readback.data.size();
    if(bytes > 0 && bytes >= sz)
    {
      encoder.add_frame(
          (const unsigned char*)m_readback.data.constData(), AV_PIX_FMT_RGBA,
          m_readback.pixelSize.width(), m_readback.pixelSize.height());
    }
  }
}

score::gfx::OutputNode::Configuration LibavEncoderNode::configuration() const noexcept
{
  return {.manualRenderingRate = 1000. / m_settings.rate};
}

void LibavEncoderNode::onRendererChange() { }

void LibavEncoderNode::stopRendering() { }

void LibavEncoderNode::setRenderer(std::shared_ptr<score::gfx::RenderList> r)
{
  m_renderer = r;
}

score::gfx::RenderList* LibavEncoderNode::renderer() const
{
  return m_renderer.lock().get();
}

void LibavEncoderNode::createOutput(score::gfx::OutputConfiguration conf)
{
  m_renderState = std::make_shared<score::gfx::RenderState>();

  m_renderState->surface = QRhiGles2InitParams::newFallbackSurface();
  QRhiGles2InitParams params;
  params.fallbackSurface = m_renderState->surface;
  score::GLCapabilities caps;
  caps.setupFormat(params.format);
  m_renderState->rhi = QRhi::create(QRhi::OpenGLES2, &params, {});
  m_renderState->renderSize = QSize(m_settings.width, m_settings.height);
  m_renderState->outputSize = m_renderState->renderSize;
  m_renderState->api = score::gfx::GraphicsApi::OpenGL;
  m_renderState->version = caps.qShaderVersion;

  auto rhi = m_renderState->rhi;
  m_texture = rhi->newTexture(
      QRhiTexture::RGBA8, m_renderState->renderSize, 1,
      QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
  m_texture->create();
  m_renderTarget = rhi->newTextureRenderTarget({m_texture});
  m_renderState->renderPassDescriptor
      = m_renderTarget->newCompatibleRenderPassDescriptor();
  m_renderTarget->setRenderPassDescriptor(m_renderState->renderPassDescriptor);
  m_renderTarget->create();

  conf.onReady();
}

void LibavEncoderNode::destroyOutput() { }

std::shared_ptr<score::gfx::RenderState> LibavEncoderNode::renderState() const
{
  return m_renderState;
}

score::gfx::OutputNodeRenderer*
LibavEncoderNode::createRenderer(score::gfx::RenderList& r) const noexcept
{
  score::gfx::TextureRenderTarget rt{
      m_texture, nullptr, nullptr, m_renderState->renderPassDescriptor, m_renderTarget};
  return new Gfx::InvertYRenderer{
      *this, rt, const_cast<QRhiReadbackResult&>(m_readback)};
}

}
