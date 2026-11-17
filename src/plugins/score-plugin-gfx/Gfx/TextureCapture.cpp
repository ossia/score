#include "TextureCapture.hpp"

#include <Gfx/GfxInputDevice.hpp>

#include <QFloat16>

#include <rhi/qrhi.h>

#include <cstring>

namespace Gfx
{
std::optional<port_index> textureProducer(const ossia::net::parameter_base& p)
{
  if(auto* t = dynamic_cast<const simple_texture_input_parameter*>(&p))
    if(t->node_id != score::gfx::invalid_node_index)
      return port_index{t->node_id, 0};
  if(auto* t = dynamic_cast<const video_texture_input_parameter*>(&p))
    if(t->node_id != score::gfx::invalid_node_index)
      return port_index{t->node_id, 0};
  return std::nullopt;
}

TextureCapture::TextureCapture(GfxContext& ctx, port_index producer)
    : m_ctx{&ctx}
    , m_readback{std::make_shared<QRhiReadbackResult>()}
    , m_completed{std::make_shared<std::atomic<int64_t>>(0)}
{
  m_readback->completed = [c = m_completed] { c->fetch_add(1, std::memory_order_release); };
  m_id = ctx.add_capture(producer, m_readback);
}

TextureCapture::~TextureCapture()
{
  if(m_ctx && m_id >= 0)
    m_ctx->remove_preview(m_id);
}

int64_t TextureCapture::frames() const noexcept
{
  return m_completed->load(std::memory_order_acquire);
}

bool TextureCapture::take(std::vector<float>& rgba, int& width, int& height)
{
  const int64_t done = frames();
  const auto now = std::chrono::steady_clock::now();
  if(done == m_taken)
  {
    if(now - m_askedAt > std::chrono::milliseconds(500))
    {
      m_ctx->want_preview_frame(m_id);
      m_askedAt = now;
    }
    return false;
  }
  m_taken = done;
  m_ctx->want_preview_frame(m_id);
  m_askedAt = now;

  const auto& rb = *m_readback;
  const int w = rb.pixelSize.width(), h = rb.pixelSize.height();
  if(w <= 0 || h <= 0)
    return false;
  const std::size_t n = std::size_t(w) * h * 4;
  switch(rb.format)
  {
    case QRhiTexture::RGBA32F:
      if(rb.data.size() < qsizetype(n * sizeof(float)))
        return false;
      rgba.resize(n);
      std::memcpy(rgba.data(), rb.data.constData(), n * sizeof(float));
      break;
    case QRhiTexture::RGBA16F: {
      if(rb.data.size() < qsizetype(n * sizeof(qfloat16)))
        return false;
      rgba.resize(n);
      const auto* src = reinterpret_cast<const qfloat16*>(rb.data.constData());
      for(std::size_t i = 0; i < n; i++)
        rgba[i] = float(src[i]);
      break;
    }
    case QRhiTexture::UnknownFormat:
    case QRhiTexture::RGBA8:
    case QRhiTexture::BGRA8: {
      if(rb.data.size() < qsizetype(n))
        return false;
      rgba.resize(n);
      const auto* src = reinterpret_cast<const uint8_t*>(rb.data.constData());
      const bool bgra = rb.format == QRhiTexture::BGRA8;
      for(std::size_t i = 0; i < n; i += 4)
      {
        rgba[i + 0] = src[i + (bgra ? 2 : 0)] / 255.f;
        rgba[i + 1] = src[i + 1] / 255.f;
        rgba[i + 2] = src[i + (bgra ? 0 : 2)] / 255.f;
        rgba[i + 3] = src[i + 3] / 255.f;
      }
      break;
    }
    default:
      return false;
  }
  width = w;
  height = h;
  return true;
}
}
