#pragma once

/**
 * @file CaptureStrategyCommon.hpp
 * @brief Backend-neutral plumbing shared by GPU-direct video CAPTURE
 *        strategies (no graphics-API headers).
 *
 * These pieces are common to every capture strategy regardless of backend:
 * the lock-free slot handoff between the vendor capture thread and the render
 * thread, and the output-texture byte-size sanity check. They carry no GL /
 * Vulkan / D3D dependency, so a Vulkan strategy can use them without dragging
 * in OpenGL private headers (which GLCaptureUpload.hpp does). GL-specific
 * upload helpers live in GLCaptureUpload.hpp, which includes this file.
 */

#include <QtGui/private/qrhi_p.h>

#include <QDebug>

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace score::gfx::interop
{

/**
 * @brief Lock-free single-producer/single-consumer slot handoff.
 *
 * The vendor capture thread publishes the index of the slot it just filled;
 * the render thread consumes it with an acquire-exchange. -1 means "nothing
 * pending". This is the single-atomic form used by the capture strategies
 * (simpler than GpuDirectCaptureSlotRing's frame-id + slot pair, which the
 * polling input-node renderer doesn't need here).
 */
struct CaptureSlotPublisher
{
  std::atomic<int> pending{-1};

  void publish(std::size_t i) noexcept
  {
    pending.store(static_cast<int>(i), std::memory_order_release);
  }
  /// Returns the published slot index, or -1 if none is pending.
  int consume() noexcept { return pending.exchange(-1, std::memory_order_acquire); }
  void reset() noexcept { pending.store(-1, std::memory_order_relaxed); }
};

/**
 * @brief Validate that an RGBA8-typed output texture's byte footprint matches
 *        the captured frame size (width * 4 * height == frameByteSize).
 *
 * Logs and returns false on mismatch so a strategy can bail before DMAing into
 * a wrongly-sized texture. @p tag is a per-strategy prefix for the warning.
 */
inline bool validateCaptureTextureBytes(
    const QRhiTexture* tex, std::uint32_t frameByteSize, const char* tag) noexcept
{
  if(!tex)
    return false;
  const auto sz = tex->pixelSize();
  const auto expected = static_cast<std::uint32_t>(sz.width()) * 4u
                        * static_cast<std::uint32_t>(sz.height());
  if(expected != frameByteSize)
  {
    qWarning() << tag << "outputTexture byte size" << expected
               << "!= captured frame size" << frameByteSize;
    return false;
  }
  return true;
}

} // namespace score::gfx::interop
