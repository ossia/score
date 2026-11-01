#include <Gfx/Widgets/TexturePreview.hpp>

#include <Gfx/GfxContext.hpp>

#include <QSysInfo>
#include <QtGui/private/qrhi_p.h>

namespace Gfx
{
TexturePreview::TexturePreview() = default;

TexturePreview::~TexturePreview()
{
  detach();
}

bool TexturePreview::attached() const noexcept
{
  return m_ctx && m_id >= 0;
}

bool TexturePreview::tapped() const noexcept
{
  return attached() && m_ctx->preview_is_tapped(m_id);
}

bool TexturePreview::onHub() const noexcept
{
  return attached() && m_ctx->preview_is_on_hub(m_id);
}

void TexturePreview::attach(GfxContext* ctx, port_index producer, QSize size)
{
  detach();
  if(!ctx)
    return;
  m_ctx = ctx;
  m_producer = producer;
  m_readback = std::make_shared<QRhiReadbackResult>();
  m_id = ctx->add_preview(producer, size, m_readback);
}

void TexturePreview::setProducer(port_index producer)
{
  if(producer == m_producer)
    return;
  m_producer = producer;
  if(attached())
    m_ctx->set_preview_producer(m_id, producer);
}

void TexturePreview::setSize(QSize size)
{
  if(attached())
    m_ctx->set_preview_size(m_id, size);
}

void TexturePreview::detach()
{
  if(attached())
    m_ctx->remove_preview(m_id);
  m_id = -1;
  m_ctx = nullptr;
  m_producer = {-1, -1};
  m_readback.reset();
}

QImage TexturePreview::image() const
{
  if(!m_readback)
    return {};
  // Taken: the next frame is read back
  if(attached())
    m_ctx->want_preview_frame(m_id);

  const auto& rb = *m_readback;
  const int w = rb.pixelSize.width(), h = rb.pixelSize.height();
  if(w <= 0 || h <= 0)
    return {};
  // The taps read back RGBA8; an output of its own reads its render format
  QImage::Format fmt{};
  int bpp{};
  switch(rb.format)
  {
    case QRhiTexture::UnknownFormat:
    case QRhiTexture::RGBA8:
      fmt = QImage::Format_RGBA8888;
      bpp = 4;
      break;
    case QRhiTexture::BGRA8:
      // B, G, R, A in memory: ARGB32 on a little-endian host
      if constexpr(QSysInfo::ByteOrder != QSysInfo::LittleEndian)
        return {};
      fmt = QImage::Format_ARGB32;
      bpp = 4;
      break;
    case QRhiTexture::RGBA16F:
      fmt = QImage::Format_RGBA16FPx4;
      bpp = 8;
      break;
    case QRhiTexture::RGBA32F:
      fmt = QImage::Format_RGBA32FPx4;
      bpp = 16;
      break;
    default:
      return {};
  }
  if(rb.data.size() < qsizetype(w) * h * bpp)
    return {};
  return QImage{
      reinterpret_cast<const unsigned char*>(rb.data.constData()), w, h, w * bpp, fmt};
}
}
