#pragma once

/**
 * @file GpuDirectCaptureStrategy.hpp
 * @brief Vendor-neutral interface for GPU-direct video CAPTURE strategies.
 *
 * Symmetric inverse of `GpuDirectStrategy` for the input direction.
 * Concrete impls live in each vendor's addon (AJA today; Magewell /
 * Ximea / Datapath planned). The plumbing helpers (`GpuRingBuffer`,
 * the slot ring atomic, `CudaP2PBridge`) are in score-plugin-gfx so
 * the per-vendor file stays ~150 lines.
 *
 * Threading contract:
 *   - `init / release / acquireForRender / releaseAfterRender /
 *      outputTexture` run on the render thread (QRhi affinity).
 *   - `ingestFrame` runs on the vendor's capture thread (AJA AC pump,
 *     Magewell event-loop, Aravis callback, ...). Slot index is the
 *     handoff currency.
 *
 * Concrete impls own the vendor pacing model: AJA waits on VBI in its
 * AC loop; Magewell / Ximea use vendor-thread push; Rivermax is PTP-
 * timed.
 */

#include <atomic>
#include <cstddef>
#include <cstdint>

class QRhi;
class QRhiTexture;

namespace score::gfx
{
struct RenderState;
}

namespace score::gfx::interop
{

struct GpuDirectCaptureStrategyConfig
{
  QRhi* rhi{};
  const score::gfx::RenderState* state{};

  /// Captured frame size in bytes (already covers row-stride padding
  /// for v210 etc.).
  std::uint32_t frameByteSize{};

  int width{};
  int height{};

  /// QRhi texture into which the strategy makes the captured frame
  /// available. Caller (the addon's input-node renderer) creates this
  /// via its packed-format decoder and hands it to the strategy at
  /// init time. Strategy DOES NOT own it.
  ///
  /// Geometry conventions are vendor-specific: byte layouts that match
  /// the vendor's native frame format, sampled as RGBA8 / BGRA8 by a
  /// vendor-aware unpacker shader.
  QRhiTexture* outputTexture{};
};

/**
 * @brief Per-graphics-API strategy for getting a captured video frame
 *        into a GPU QRhiTexture without CPU staging.
 *
 * Implementation pattern: a fixed-depth ring of buffers / pinned host
 * memory that the vendor capture thread fills, then either a CUDA copy
 * (tier-3 RDMA path) or a DMA-bridge copy (DVP) into the renderer's
 * texture.
 */
struct GpuDirectCaptureStrategy
{
  virtual ~GpuDirectCaptureStrategy() = default;

  /// Short identifier for logs, e.g. "DVP-D3D11" / "RDMA-GL/T3" /
  /// "Magewell-NVRDMA/GL".
  virtual const char* name() const noexcept = 0;

  virtual bool init(const GpuDirectCaptureStrategyConfig& cfg) = 0;
  virtual void release() = 0;

  /// Sysmem-slot pool size. Strategies use a small fixed depth (3)
  /// to overlap the vendor DMA, the bridge DMA, and renderer sampling
  /// without locks.
  virtual std::size_t slotCount() const noexcept = 0;

  /// Pointer the vendor capture thread hands its per-frame submission
  /// API. On DVP this is a page-locked host buffer; on tier-3 RDMA
  /// (AJA Linux, Magewell+NVRDMA) it's a GPU device pointer pinned
  /// for P2P. Vendor-agnostic at the interface — the semantics depend
  /// on the strategy.
  virtual void* slotBuffer(std::size_t i) const noexcept = 0;

  /// Called on the vendor capture thread after the captured frame has
  /// landed in slot `i`. Strategy may issue its bridge copy (DVP) or
  /// publish a slot index to the renderer (tier-3 RDMA).
  virtual bool ingestFrame(std::size_t i) = 0;

  /// QRhi texture downstream nodes sample. Strategy-owned or caller-
  /// owned depending on the impl; convenience accessor either way.
  virtual QRhiTexture* outputTexture() const noexcept = 0;

  /// Render-thread bracket around sampling outputTexture(). DVP impls
  /// use this to handshake with the vendor's API/DVP semaphore; tier-3
  /// RDMA impls use it to copy buffer → texture on the render thread.
  virtual void acquireForRender() = 0;
  virtual void releaseAfterRender() = 0;
};

/**
 * @brief Lock-free SPSC handoff between the capture thread and the
 *        renderer. Capture thread bumps `latestFrameId` and publishes
 *        the slot index; renderer polls and consumes.
 *
 *   producer: writeIdx = (writeIdx + 1) % N;
 *             ingest into slotBuffer(writeIdx);
 *             latestSlot.store(writeIdx, release);
 *             latestFrameId.store(++currentFrame, release);
 *   consumer: id = latestFrameId.load(acquire);
 *             if (id == lastIngested) skip;
 *             slot = latestSlot.load(acquire);
 *             ingest from slot;
 *             lastIngested = id;
 */
struct GpuDirectCaptureSlotRing
{
  std::atomic<std::uint64_t> latestFrameId{0};
  std::atomic<std::size_t> latestSlot{0};
};

} // namespace score::gfx::interop
