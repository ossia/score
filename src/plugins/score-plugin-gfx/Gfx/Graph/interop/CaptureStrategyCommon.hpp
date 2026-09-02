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
 * (simpler than VideoCaptureSlotRing's frame-id + slot pair, which the
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
 * @brief Ownership bookkeeping for capture strategies that sample the
 *        producer's own buffers instead of copying out of them.
 *
 * A borrowed slot must not go back to the producer's device while the GPU may
 * still be reading it. Exactly one party owns a slot at a time:
 *
 *   producer  ingest(i)    -- slot i becomes render-owned; a slot that was
 *                             published but never consumed is handed straight
 *                             back (it was never bound).
 *   render    acquire()    -- takes the published slot and retires the one it
 *                             displaces.
 *   render                 -- a retired slot only becomes returnable after
 *                             `retireDepth` further acquisitions, which is
 *                             where QRhi guarantees the frame that last
 *                             sampled it has completed. Acquisitions are
 *                             counted, not QRhi frames, and there is at most
 *                             one per frame, so the count is conservative.
 *   producer  takeReturned -- drains the bitmask of slots safe to give back.
 *
 * A producer that never calls takeReturned() starves its own queue; one that
 * returns a slot without it corrupts the frame being displayed.
 */
struct BorrowedSlotTracker
{
  static constexpr std::size_t kMaxSlots = 32;

  CaptureSlotPublisher publisher;

  /// `framesInFlight + 1`, from QRhi::resourceLimit(QRhi::FramesInFlight).
  std::size_t retireDepth{1};

  /// Producer thread. Returns the slot displaced without ever being bound
  /// (-1 if none), which the caller must hand back to its device.
  int ingest(std::size_t i) noexcept
  {
    const int displaced = publisher.pending.exchange(int(i), std::memory_order_acq_rel);
    if(displaced >= 0 && displaced != int(i))
    {
      returns.fetch_or(1u << unsigned(displaced), std::memory_order_release);
      return displaced;
    }
    return -1;
  }

  /// Render thread. The slot to bind now, or -1 when nothing is pending.
  int acquire() noexcept
  {
    const int slot = publisher.consume();
    if(slot < 0)
      return -1;

    ++acquisitions;
    if(held >= 0 && held != slot && retireN < kMaxSlots)
      retire[retireN++] = Retired{held, acquisitions};
    held = slot;

    std::uint32_t freed = 0;
    std::size_t keep = 0;
    for(std::size_t i = 0; i < retireN; ++i)
    {
      if(acquisitions - retire[i].at >= retireDepth)
        freed |= 1u << unsigned(retire[i].slot);
      else
        retire[keep++] = retire[i];
    }
    retireN = keep;
    if(freed)
      returns.fetch_or(freed, std::memory_order_release);
    return slot;
  }

  /// Producer thread. Bitmask of slots the renderer has finished with.
  std::uint32_t takeReturned() noexcept
  {
    return returns.exchange(0, std::memory_order_acquire);
  }

  void reset() noexcept
  {
    publisher.reset();
    returns.store(0, std::memory_order_relaxed);
    held = -1;
    acquisitions = 0;
    retireN = 0;
  }

private:
  struct Retired
  {
    int slot{};
    std::uint64_t at{};
  };
  std::atomic<std::uint32_t> returns{0};
  Retired retire[kMaxSlots]{};
  std::size_t retireN{0};
  int held{-1};
  std::uint64_t acquisitions{0};
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
