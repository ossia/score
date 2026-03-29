#include "LibavEncoderNode.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/encoders/I420.hpp>
#include <Gfx/Graph/encoders/NV12.hpp>
#include <Gfx/Graph/encoders/UYVY.hpp>
#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/Libav/LibavEncoder.hpp>

#include <score/gfx/OpenGL.hpp>
#include <score/gfx/QRhiGles2.hpp>

#include <QOffscreenSurface>

extern "C" {
#include <libavutil/pixdesc.h>
}

namespace Gfx
{

// Map FFmpeg pixel format name to GPU encoder
static std::unique_ptr<score::gfx::GPUVideoEncoder>
makeEncoderForPixfmt(const QString& pixfmt)
{
  if(pixfmt == "yuv420p" || pixfmt == "yuvj420p")
    return std::make_unique<score::gfx::I420Encoder>();
  if(pixfmt == "nv12")
    return std::make_unique<score::gfx::NV12Encoder>();
  if(pixfmt == "uyvy422")
    return std::make_unique<score::gfx::UYVYEncoder>();
  return nullptr;
}

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

    if(m_encoder[0])
    {
      // GPU encoder path: double-buffered (same pattern as GStreamer/NDI)
      auto& currentEnc = *m_encoder[m_encoderIdx];
      auto& prevEnc = *m_encoder[m_encoderIdx ^ 1];

      currentEnc.exec(*rhi, *cb);
      rhi->endOffscreenFrame();

      // Push the PREVIOUS frame's readback to the encoder
      if(prevEnc.readback(0).data.size() > 0)
      {
        const int planeCount = prevEnc.planeCount();
        const unsigned char* planes[4] = {};
        int strides[4] = {};

        for(int i = 0; i < planeCount; i++)
        {
          auto& rb = prevEnc.readback(i);
          planes[i] = (const unsigned char*)rb.data.constData();
          strides[i] = rb.pixelSize.width()
                       * ((i == 0 || planeCount == 1)
                              ? (planeCount == 1 ? 4 : 1)  // UYVY=4bpp, Y=1bpp
                              : (planeCount == 2 ? 2 : 1)); // UV=2bpp (NV12) or U/V=1bpp (I420)
        }

        // Compute strides from readback data size and height
        for(int i = 0; i < planeCount; i++)
        {
          auto& rb = prevEnc.readback(i);
          if(rb.pixelSize.height() > 0)
            strides[i] = rb.data.size() / rb.pixelSize.height();
        }

        encoder.add_frame_converted(
            planes, strides, planeCount,
            m_settings.width, m_settings.height);
      }

      m_encoderIdx ^= 1;
    }
    else
    {
      // Standard RGBA path with double-buffered readback
      rhi->endOffscreenFrame();

      auto& readback = *m_currentReadback;
      int sz = readback.pixelSize.width() * readback.pixelSize.height() * 4;
      int bytes = readback.data.size();
      if(bytes > 0 && bytes >= sz)
      {
        encoder.add_frame(
            (const unsigned char*)readback.data.constData(), AV_PIX_FMT_RGBA,
            readback.pixelSize.width(), readback.pixelSize.height());
      }

      // Swap readback buffer for next frame
      m_currentReadback = (m_currentReadback == &m_readback[0])
                              ? &m_readback[1]
                              : &m_readback[0];
      if(m_inv_y_renderer)
        m_inv_y_renderer->updateReadback(*m_currentReadback);
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

  m_depthStencil = rhi->newRenderBuffer(
      QRhiRenderBuffer::DepthStencil, m_renderState->renderSize, 1);
  m_depthStencil->create();

  QRhiTextureRenderTargetDescription desc{m_texture};
  desc.setDepthStencilBuffer(m_depthStencil);
  m_renderTarget = rhi->newTextureRenderTarget(desc);
  m_renderState->renderPassDescriptor
      = m_renderTarget->newCompatibleRenderPassDescriptor();
  m_renderTarget->setRenderPassDescriptor(m_renderState->renderPassDescriptor);
  m_renderTarget->create();

  // Try to create GPU encoder for the target pixel format
  if(!m_settings.video_converted_pixfmt.isEmpty())
  {
    m_encoder[0] = makeEncoderForPixfmt(m_settings.video_converted_pixfmt);
    m_encoder[1] = makeEncoderForPixfmt(m_settings.video_converted_pixfmt);

    if(m_encoder[0] && m_encoder[1] && rhi)
    {
      // Build color conversion shader from settings
      auto input_trc = static_cast<AVColorTransferCharacteristic>(m_settings.input_transfer);
      auto colorShader = score::gfx::colorMatrixOut(
          AVCOL_SPC_BT709, AVCOL_TRC_BT709, AVCOL_RANGE_JPEG, AVCOL_PRI_BT709,
          input_trc);

      m_encoder[0]->init(*rhi, *m_renderState, m_texture,
                         m_settings.width, m_settings.height, colorShader);
      m_encoder[1]->init(*rhi, *m_renderState, m_texture,
                         m_settings.width, m_settings.height, colorShader);
    }
    else
    {
      m_encoder[0].reset();
      m_encoder[1].reset();
    }
  }

  conf.onReady();
}

void LibavEncoderNode::destroyOutput()
{
  for(auto& enc : m_encoder)
  {
    if(enc)
    {
      enc->release();
      enc.reset();
    }
  }

  if(m_renderState)
  {
    delete m_renderTarget;
    m_renderTarget = nullptr;
    delete m_renderState->renderPassDescriptor;
    m_renderState->renderPassDescriptor = nullptr;
    delete m_depthStencil;
    m_depthStencil = nullptr;
    delete m_texture;
    m_texture = nullptr;
    delete m_renderState->rhi;
    m_renderState->rhi = nullptr;
    delete m_renderState->surface;
    m_renderState->surface = nullptr;
    m_renderState.reset();
  }
}

std::shared_ptr<score::gfx::RenderState> LibavEncoderNode::renderState() const
{
  return m_renderState;
}

score::gfx::OutputNodeRenderer*
LibavEncoderNode::createRenderer(score::gfx::RenderList& r) const noexcept
{
  score::gfx::TextureRenderTarget rt{
      m_texture, nullptr, nullptr, m_renderState->renderPassDescriptor, m_renderTarget};

  if(m_encoder[0])
  {
    // GPU encoder active: BasicRenderer (no Y-flip, no readback)
    return new Gfx::BasicRenderer{rt, *m_renderState, *this};
  }

  // Standard RGBA: InvertYRenderer with double-buffered readback
  return const_cast<Gfx::InvertYRenderer*&>(m_inv_y_renderer)
      = new Gfx::InvertYRenderer{
          *this, rt, const_cast<QRhiReadbackResult&>(m_readback[0])};
}

}
