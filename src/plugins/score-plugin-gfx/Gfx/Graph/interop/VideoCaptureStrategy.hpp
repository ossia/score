#pragma once

/**
 * @file VideoCaptureStrategy.hpp
 * @brief Vendor-neutral interface for GPU-direct video CAPTURE strategies.
 *
 * Symmetric inverse of `VideoOutputStrategy` for the input direction.
 * Concrete impls live in each vendor's addon (AJA today; Magewell /
 * Ximea / Datapath planned). The plumbing helpers (`ImportedGpuBufferRing`,
 * the slot ring atomic, `CudaInterop`) are in score-plugin-gfx so
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
class QRhiResourceUpdateBatch;

namespace score::gfx
{
struct RenderState;
}

namespace score::gfx::interop
{

struct VideoCaptureStrategyConfig
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
 * (RDMA path) or a DMA-bridge copy (DVP) into the renderer's
 * texture.
 */
struct VideoCaptureStrategy
{
  virtual ~VideoCaptureStrategy() = default;

  /// Short identifier for logs, e.g. "DVP-D3D11" / "RDMA-GL" /
  /// "Magewell-NVRDMA/GL".
  virtual const char* name() const noexcept = 0;

  virtual bool init(const VideoCaptureStrategyConfig& cfg) = 0;
  virtual void release() = 0;

  /// Sysmem-slot pool size. Strategies use a small fixed depth (3)
  /// to overlap the vendor DMA, the bridge DMA, and renderer sampling
  /// without locks.
  virtual std::size_t slotCount() const noexcept = 0;

  /// Pointer the vendor capture thread hands its per-frame submission
  /// API. On DVP this is a page-locked host buffer; on RDMA
  /// (AJA Linux, Magewell+NVRDMA) it's a GPU device pointer pinned
  /// for P2P. Vendor-agnostic at the interface — the semantics depend
  /// on the strategy.
  virtual void* slotBuffer(std::size_t i) const noexcept = 0;

  /// Called on the vendor capture thread after the captured frame has
  /// landed in slot `i`. Strategy may issue its bridge copy (DVP) or
  /// publish a slot index to the renderer (RDMA).
  virtual bool ingestFrame(std::size_t i) = 0;

  /// QRhi texture downstream nodes sample. Strategy-owned or caller-
  /// owned depending on the impl; convenience accessor either way.
  virtual QRhiTexture* outputTexture() const noexcept = 0;

  /// The texture holding the frame just made available by the most recent
  /// acquireForRender(). For strategies that upload into a single fixed
  /// texture this is always outputTexture(); strategies that double-buffer
  /// the sampled resource (Vulkan, to avoid the capture-write /
  /// render-sample race) return the freshest completed slot's texture, and
  /// the renderer rebinds its passes to it when it changes. Called on the
  /// render thread, after acquireForRender().
  virtual QRhiTexture* currentTexture() const noexcept { return outputTexture(); }

  /// Render-thread bracket around sampling outputTexture(). DVP impls
  /// use this to handshake with the vendor's API/DVP semaphore; RDMA
  /// impls use it to copy buffer → texture on the render thread.
  virtual void acquireForRender() = 0;
  virtual void releaseAfterRender() = 0;

  /// Batch-aware variant, called by the renderer with the active QRhi
  /// resource-update batch. Backends that upload through a raw graphics API
  /// (GL glTexSubImage2D, DVP/D3D copies) ignore the batch and fall through to
  /// the no-arg acquireForRender(). A portable CPU-staging strategy overrides
  /// this to issue a backend-neutral QRhiResourceUpdateBatch::uploadTexture
  /// (the only path that works on Vulkan/Metal/D3D where there is no raw upload
  /// API at hand).
  virtual void acquireForRender(QRhiResourceUpdateBatch&) { acquireForRender(); }
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
struct VideoCaptureSlotRing
{
  std::atomic<std::uint64_t> latestFrameId{0};
  std::atomic<std::size_t> latestSlot{0};

  // Live input-format channel. The capture thread publishes a newly detected
  // wire geometry and bumps formatGeneration; the render thread polls it beside
  // latestFrameId and reallocates its size-dependent GPU resources. Read via a
  // seqlock: capture writes the fields then bumps the generation (even = stable,
  // odd = mid-write); the render side reads the generation, the fields, then
  // re-reads the generation and retries on mismatch.
  std::atomic<std::uint64_t> formatGeneration{0};
  std::atomic<std::int32_t> detectedWidth{0};
  std::atomic<std::int32_t> detectedHeight{0};
  std::atomic<std::int32_t> detectedPixelFormat{-1}; // AVPixelFormat; -1 = unset
  std::atomic<double> detectedRate{0.0};

  // Capture-thread side: publish a detected format change. No-op if the
  // geometry is unchanged, so a per-frame poll can call it unconditionally.
  bool publishFormat(int w, int h, int pixfmt, double rate) noexcept
  {
    if(w == detectedWidth.load(std::memory_order_relaxed)
       && h == detectedHeight.load(std::memory_order_relaxed)
       && pixfmt == detectedPixelFormat.load(std::memory_order_relaxed))
      return false;
    const auto g = formatGeneration.load(std::memory_order_relaxed);
    formatGeneration.store(g + 1, std::memory_order_release); // odd: mid-write
    detectedWidth.store(w, std::memory_order_relaxed);
    detectedHeight.store(h, std::memory_order_relaxed);
    detectedPixelFormat.store(pixfmt, std::memory_order_relaxed);
    detectedRate.store(rate, std::memory_order_relaxed);
    formatGeneration.store(g + 2, std::memory_order_release); // even: stable
    return true;
  }

  struct DetectedFormat
  {
    std::uint64_t generation{0};
    int width{0}, height{0}, pixfmt{-1};
    double rate{0.0};
  };

  // Render-thread side: consistent snapshot of the latest published format.
  DetectedFormat loadFormat() const noexcept
  {
    DetectedFormat f;
    for(;;)
    {
      const auto g0 = formatGeneration.load(std::memory_order_acquire);
      if(g0 & 1u)
        continue; // writer mid-update
      f.width = detectedWidth.load(std::memory_order_relaxed);
      f.height = detectedHeight.load(std::memory_order_relaxed);
      f.pixfmt = detectedPixelFormat.load(std::memory_order_relaxed);
      f.rate = detectedRate.load(std::memory_order_relaxed);
      std::atomic_thread_fence(std::memory_order_acquire);
      if(formatGeneration.load(std::memory_order_relaxed) == g0)
      {
        f.generation = g0;
        return f;
      }
    }
  }
};

} // namespace score::gfx::interop
