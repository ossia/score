#pragma once
/**
 * @file HostFramePool.hpp
 * @brief App-owned host frame pool behind a FrameMemoryProvider.
 *
 * The direct-readback destination for vendors whose submit is a synchronous
 * DMA from an arbitrary host pointer (AJA AutoCirculateTransfer, Bluefish
 * bfDmaWrite, Deltacast slot buffers): unlike DeckLink there is no SDK frame
 * object to wrap, so the pool allocates the frames itself, sized and aligned
 * so the GPU can wrap each one as a readback target, and pins them through the
 * vendor's VendorDmaRegistrar for the card's DMA engine.
 *
 * Each frame is one whole allocation: bytes == regionBase, no interior offset.
 * acquire() pops a free frame (an empty pool is that render tick's
 * back-pressure drop); recycle() returns a frame after the vendor's submit
 * completes or on any discard/cancel path. Because the vendor's DMA is
 * synchronous, a frame is reusable the moment submit returns — the pool never
 * tracks hardware completions.
 *
 * Allocation granule: VirtualAlloc in whole 64 KiB on Windows (the only
 * memory D3D12's OpenExistingHeapFromAddress accepts, at its allocation
 * granularity), 4096-aligned elsewhere (VK_EXT_external_memory_host /
 * GL_AMD_pinned_memory page requirement).
 *
 * Threading: acquire() runs on the render thread, recycle() on the vendor's
 * pump thread; the free list is mutex-guarded. allocate()/release() are
 * init/teardown only.
 */
#include <score_plugin_gfx_export.h>

#include <Gfx/Graph/interop/CpuStagedVideoOutput.hpp>
#include <Gfx/Graph/interop/VendorDmaRegistrar.hpp>

#include <cstddef>
#include <mutex>
#include <vector>

namespace score::gfx::interop
{

class SCORE_PLUGIN_GFX_EXPORT HostFramePool
{
public:
  HostFramePool();
  ~HostFramePool();

  HostFramePool(const HostFramePool&) = delete;
  HostFramePool& operator=(const HostFramePool&) = delete;

  /// Allocate + pin `frames` buffers of `frameBytes` (rounded up to the
  /// platform wrap granule). Returns false with everything rolled back when
  /// any allocation or pin fails.
  bool allocate(std::size_t frameBytes, int frames, VendorDmaRegistrar reg);

  /// Unpin + free. Call with every frame back in the pool (pump stopped) and
  /// while the registrar's device handle is still open.
  void release();

  bool valid() const noexcept { return !m_frames.empty(); }

  /// FrameMemoryProvider over this pool. Valid while the pool is alive.
  FrameMemoryProvider provider();

  /// Whether `p` is one of this pool's frame base pointers.
  bool owns(const void* p) const noexcept;

  /// Return an acquired frame to the free list; no-op for pointers the pool
  /// does not own (staging-ring or GPU-direct frame pointers pass through).
  void recycle(void* p) noexcept;

private:
  VendorFrameMemory acquire() noexcept;

  struct Frame
  {
    void* base{};
    std::size_t bytes{};
    bool inUse{};
  };

  std::mutex m_mutex;
  std::vector<Frame> m_frames; ///< stable after allocate(); only inUse mutates
  VendorDmaRegistrar m_reg;
};

} // namespace score::gfx::interop
