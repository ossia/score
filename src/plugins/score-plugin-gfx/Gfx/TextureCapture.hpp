#pragma once
#include <Gfx/Widgets/TexturePreview.hpp>

#include <QString>

#include <score_plugin_gfx_export.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace ossia::net
{
class parameter_base;
}

namespace Gfx
{
//! The gfx node and port that a device's texture parameter (a camera, a
//! depth camera stream) renders, when it is one.
SCORE_PLUGIN_GFX_EXPORT std::optional<port_index>
textureProducer(const ossia::net::parameter_base& p);

/**
 * @brief Full-resolution frames of a texture producer, read back on the CPU.
 *
 * For measurement (projector calibration): the texture the producer publishes
 * is read back at its own size in RGBA32F, through the same preview path as
 * the editor's thumbnails (GfxContext::add_capture), so it works with the
 * score stopped and without a process in the document. Never resampled: when
 * that texture cannot be read, state() says so instead.
 *
 * One readback at a time, and only when asked: request(), then take() once
 * frames() moved.
 */
class SCORE_PLUGIN_GFX_EXPORT TextureCapture
{
public:
  enum class State
  {
    //! Not read back yet; reason() may say what it waits for
    Waiting,
    //! A render list reads the producer's texture back on request()
    Capturing,
    //! It cannot be read back: reason() says why
    Unavailable
  };

  TextureCapture(GfxContext& ctx, port_index producer);
  ~TextureCapture();
  TextureCapture(const TextureCapture&) = delete;
  TextureCapture& operator=(const TextureCapture&) = delete;

  //! Another producer, e.g. when the device was created again.
  void setProducer(port_index producer);
  port_index producer() const noexcept { return m_preview.producer(); }

  State state() const noexcept;
  QString reason() const;

  //! Reads the next frame back.
  void request();
  //! The frame read back since the last call, RGBA float, top row first.
  bool take(std::vector<float>& rgba, int& width, int& height);
  //! The readbacks completed so far.
  int64_t frames() const noexcept;

private:
  TexturePreview m_preview;
  int64_t m_taken{};
};
}
