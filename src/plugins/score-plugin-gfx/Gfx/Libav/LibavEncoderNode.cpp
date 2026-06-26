#include "LibavEncoderNode.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/encoders/I420.hpp>
#include <Gfx/Graph/encoders/NV12.hpp>
#include <Gfx/Graph/encoders/UYVY.hpp>
#include <Gfx/InvertYRenderer.hpp>

#include <cmath>
#include <cstdint>
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

// Bit depth of an ffmpeg pixel-format name (8 if unknown).
static int pixfmtDepth(const QString& name)
{
  const AVPixelFormat f = av_get_pix_fmt(name.toUtf8().constData());
  if(f == AV_PIX_FMT_NONE)
    return 8;
  const AVPixFmtDescriptor* d = av_pix_fmt_desc_get(f);
  return (d && d->nb_components > 0) ? d->comp[0].depth : 8;
}

// IEEE-754 binary16 -> normalized uint16 (clamped to [0,1]).
static inline uint16_t halfToU16(uint16_t h)
{
  const uint32_t exp = (h >> 10) & 0x1Fu, mant = h & 0x3FFu;
  if(h & 0x8000u)
    return 0; // negative -> 0
  float v;
  if(exp == 0u)
    v = std::ldexp(float(mant), -24);
  else if(exp == 31u)
    v = 1.f; // inf/nan -> white
  else
    v = std::ldexp(float(mant | 0x400u), int(exp) - 25);
  if(v >= 1.f)
    return 65535;
  return uint16_t(v * 65535.f + 0.5f);
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
      const int w = readback.pixelSize.width(), h = readback.pixelSize.height();
      const int bpp = m_tenBit ? 8 : 4; // RGBA16F (4x half) vs RGBA8
      const int sz = w * h * bpp;
      const int bytes = readback.data.size();
      if(bytes > 0 && bytes >= sz)
      {
        if(m_tenBit)
        {
          // RGBA16F (half) -> RGBA64 (uint16) so swscale gets real 10-bit.
          const auto* src = (const uint16_t*)readback.data.constData();
          const size_t n = size_t(w) * h * 4;
          m_rgba64.resize(n);
          for(size_t i = 0; i < n; ++i)
            m_rgba64[i] = halfToU16(src[i]);
          encoder.add_frame(
              (const unsigned char*)m_rgba64.data(), AV_PIX_FMT_RGBA64, w, h);
        }
        else
        {
          encoder.add_frame(
              (const unsigned char*)readback.data.constData(), AV_PIX_FMT_RGBA,
              w, h);
        }
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
  m_renderState = score::gfx::createRenderState(
      conf.graphicsApi, QSize(m_settings.width, m_settings.height), nullptr);
  if(!m_renderState || !m_renderState->rhi)
  {
    qWarning() << "LibavEncoderNode: failed to create QRhi";
    m_renderState.reset();
    return;
  }
  m_renderState->outputSize = m_renderState->renderSize;

  auto rhi = m_renderState->rhi;
  // A 10-bit target codec (e.g. yuv422p10le / ProRes) with no dedicated 8-bit
  // GPU encoder gets a >8-bit scene render target so the readback -> swscale
  // carries real 10-bit precision rather than 8-bit upsampled.
  m_tenBit = pixfmtDepth(m_settings.video_converted_pixfmt) > 8
             && !makeEncoderForPixfmt(m_settings.video_converted_pixfmt);
  m_texture = rhi->newTexture(
      m_tenBit ? QRhiTexture::RGBA16F : QRhiTexture::RGBA8,
      m_renderState->renderSize, 1,
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
    // Persist-across-rebuild contract: registry survives RL teardown,
    // so we tear down its QRhi resources here BEFORE
    // RenderState::destroy() (called below) frees the device.
    releaseRegistry();

    delete m_renderTarget;
    m_renderTarget = nullptr;
    delete m_renderState->renderPassDescriptor;
    m_renderState->renderPassDescriptor = nullptr;
    delete m_depthStencil;
    m_depthStencil = nullptr;
    delete m_texture;
    m_texture = nullptr;
    // RenderState::destroy() flushes the pipeline cache via preRhiDestroy
    // and then deletes rhi + surface. Doing the deletes manually (the
    // previous approach) bypassed the cache flush.
    m_renderState->destroy();
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
      .texture = m_texture,
      .renderPass = m_renderState->renderPassDescriptor,
      .renderTarget = m_renderTarget};

  //       .texture = m_texture,
  //       .renderPass = m_renderState->renderPassDescriptor,
  //       .renderTarget = m_renderTarget};
  //   return new Gfx::InvertYRenderer{
  //       *this, rt, const_cast<QRhiReadbackResult&>(m_readback)};
  //
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
