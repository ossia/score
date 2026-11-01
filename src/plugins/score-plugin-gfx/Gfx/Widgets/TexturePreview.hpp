#pragma once
#include <ossia/gfx/port_index.hpp>

#include <QImage>
#include <QPointer>
#include <QRectF>
#include <QSize>

#include <score_plugin_gfx_export.h>

#include <cstdint>
#include <memory>

struct QRhiReadbackResult;

namespace Gfx
{
class GfxContext;
using port_index = ossia::gfx::port_index;

/**
 * @brief What a texture outlet produces, as an image any view can paint: an
 *        inspector widget, an item of the scene.
 *
 * When an output of the patch renders the producer, its render list reads the
 * outlet back as it renders it (score::gfx::PreviewTap): the preview costs one
 * small pass and readback, and shows what the output shows. Otherwise the
 * preview has an offscreen output of its own, which pulls the producer: it
 * then renders even when nothing else consumes it. The GfxContext switches
 * between the two as outputs come and go; image() is the latest frame.
 */
class SCORE_PLUGIN_GFX_EXPORT TexturePreview
{
public:
  TexturePreview();
  ~TexturePreview();
  TexturePreview(const TexturePreview&) = delete;
  TexturePreview& operator=(const TexturePreview&) = delete;

  //! Starts previewing `producer`, at most `size` pixels. Replaces any
  //! previous one.
  void attach(GfxContext* ctx, port_index producer, QSize size);
  //! Rewires the preview to another producer, or to none.
  void setProducer(port_index producer);
  void setSize(QSize size);
  void detach();

  bool attached() const noexcept;
  port_index producer() const noexcept { return m_producer; }
  //! Whether a render list of the patch reads it, rather than an output of
  //! its own.
  bool tapped() const noexcept;
  //! Whether the shared output of the previews renders it.
  bool onHub() const noexcept;

  //! The latest frame, empty until one was read back. It keeps the aspect of
  //! the texture. It refers to the readback's memory: copy it to keep it past
  //! the next frame. Taking it asks for the next frame to be read back.
  QImage image() const;

  //! Where an image of `size` goes in `box`: as large as fits, centred.
  static QRectF fit(const QRectF& box, QSize size) noexcept
  {
    if(size.isEmpty())
      return box;
    const QSizeF s = QSizeF(size).scaled(box.size(), Qt::KeepAspectRatio);
    return {box.center().x() - s.width() / 2., box.center().y() - s.height() / 2., s.width(), s.height()};
  }

private:
  QPointer<GfxContext> m_ctx;
  port_index m_producer{-1, -1};
  int32_t m_id{-1};
  std::shared_ptr<QRhiReadbackResult> m_readback;
};
}
