#pragma once
#include <Gfx/GfxContext.hpp>

#include <score_plugin_gfx_export.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

struct QRhiReadbackResult;
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
 * For measurement (projector calibration): the producer is rendered and read
 * back at its own size in RGBA32F, through the same preview path as the
 * editor's thumbnails, so it works with the score stopped and without a
 * process in the document. One readback is in flight at a time: take() asks
 * for the next frame.
 */
class SCORE_PLUGIN_GFX_EXPORT TextureCapture
{
public:
  TextureCapture(GfxContext& ctx, port_index producer);
  ~TextureCapture();
  TextureCapture(const TextureCapture&) = delete;
  TextureCapture& operator=(const TextureCapture&) = delete;

  //! The frame read back since the last call, RGBA float, top row first.
  bool take(std::vector<float>& rgba, int& width, int& height);
  int64_t frames() const noexcept;

private:
  GfxContext* m_ctx{};
  int32_t m_id{-1};
  std::shared_ptr<QRhiReadbackResult> m_readback;
  std::shared_ptr<std::atomic<int64_t>> m_completed;
  int64_t m_taken{};
};
}
