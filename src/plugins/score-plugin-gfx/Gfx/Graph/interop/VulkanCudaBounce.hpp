#pragma once

/**
 * @file VulkanCudaBounce.hpp
 * @brief CUDA-VMM bounce buffers imported into Vulkan — the Vulkan tier-3
 *        bridge between QRhi-owned resources and a vendor DMA engine.
 *
 * The GL tier-3 path bridges via cuGraphicsGLRegisterBuffer + one DtoD copy
 * into a CUDA-owned bounce (only CUDA-allocator memory is pinnable by
 * DMABufferLock-style vendor APIs). Vulkan has no equivalent of registering
 * a QRhi buffer with CUDA (QRhi VMA-allocates without export info), so the
 * bridge is inverted: allocate the bounce with CUDA VMM (gpuDirectRDMACapable,
 * POSIX-fd exportable — verified pinnable and transfer-correct by probe),
 * import that same memory INTO Vulkan as a VkBuffer, and record ONE
 * vkCmdCopyBuffer (or vkCmdCopyBufferToImage) per frame between the QRhi
 * resource and the bounce. Zero sysmem in the path, symmetric with GL.
 *
 * Per-slot cost: BAR1 aperture (256 MiB on most Quadros) — callers size
 * slot counts like the GL path does at large rasters.
 *
 * Ordering: callers use QRhi::finish() (output) or the vendor's blocking
 * DMA + slot publish (capture), exactly like the GL path. Timeline
 * semaphores can replace finish() later without changing this interface.
 */

#include <score_plugin_gfx_export.h>

#include <QtGui/qtguiglobal.h>

#if QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
#define SCORE_HAS_VULKAN_CUDA_BOUNCE 1

#include <Gfx/Graph/interop/CudaInterop.h>
#include <Gfx/Graph/interop/VkCudaSemaphore.hpp>
#include <Gfx/Graph/interop/VkExternalMemoryHelpers.hpp>

#include <cstdint>
#include <vector>

class QRhi;
class QRhiBuffer;
class QRhiCommandBuffer;
class QRhiTexture;

namespace score::gfx::interop
{

struct VulkanCudaBounceConfig
{
  QRhi* rhi{};
  int slotCount{1};
  std::size_t frameBytes{};
  const char* debugName{"vk-cuda-bounce"};
};

class SCORE_PLUGIN_GFX_EXPORT VulkanCudaBounce
{
public:
  VulkanCudaBounce() = default;
  ~VulkanCudaBounce() { release(); }
  VulkanCudaBounce(const VulkanCudaBounce&) = delete;
  VulkanCudaBounce& operator=(const VulkanCudaBounce&) = delete;

  /** Allocate + import all slots. Requires a live Vulkan QRhi. */
  bool init(const VulkanCudaBounceConfig& cfg);
  void release();

  bool valid() const noexcept { return !m_slots.empty(); }
  std::size_t slotCount() const noexcept { return m_slots.size(); }

  /** CUDA device pointer of slot i's pinned bounce — what the vendor
   *  pins/DMAs. CUDA-allocator memory (cuMemAlloc), never Vulkan-owned. */
  void* cudaPtr(std::size_t i) const noexcept
  {
    return i < m_slots.size() ? m_slots[i].bouncePtr : nullptr;
  }

  /** Output direction: DtoD the CUDA view of the exportable VkBuffer into
   *  the pinned bounce (call after the vkCmdCopyBuffer frame finished). */
  bool flushToBounce(std::size_t i, std::size_t bytes);

  /** Capture direction: DtoD the pinned bounce into the CUDA view of the
   *  exportable VkBuffer (call before recording the buffer→image copy). */
  bool flushFromBounce(std::size_t i, std::size_t bytes);

  /** Record `bytes` copy from a QRhi storage buffer's VkBuffer into slot i.
   *  Must run inside an open QRhi frame, outside any pass (uses
   *  beginExternal). srcVkBuffer is the *dereferenced* VkBuffer handle. */
  bool recordCopyToSlot(
      QRhiCommandBuffer& cb, void* srcVkBuffer, std::size_t bytes,
      std::size_t i);

  /** Debug: print the first bytes of slot i via CUDA (bounce content
   *  ground truth after a finish()). */
  void debugPeek(std::size_t i, const char* tag);

  /** True when the exported timeline semaphore + queue handle are live and
   *  the signal/wait fast path can replace QRhi::finish(). */
  bool timelineSupported() const noexcept
  {
    return m_gfxQueue && m_fnQueueSubmit && m_sem.valid();
  }

  /** GPU-side "copy done" signal: submits an EMPTY batch on the QRhi
   *  graphics queue that signals the timeline semaphore at the next value.
   *  Per the Vulkan spec a signal operation's first synchronization scope
   *  includes everything earlier in submission order on the queue — i.e.
   *  the just-submitted copy frame. Call after endOffscreenFrame(). */
  bool signalCopyDoneOnQueue();

  /** Schedule a wait for the last signalCopyDoneOnQueue() value on the
   *  bridge's CUDA stream (async; the next copy_dtod on that stream is
   *  ordered after it). */
  bool waitCopyDoneOnStream();

  /** Record a buffer→image copy of slot i into a QRhi texture (the decode
   *  input). Handles layout transitions + setNativeLayout. `texelBytes` is
   *  bytes per texel of the texture's format (the wire frame must be
   *  exactly width*height*texelBytes). */
  bool recordCopySlotToTexture(
      QRhiCommandBuffer& cb, QRhiTexture* dst, int width, int height,
      int texelBytes, std::size_t i);

private:
  struct Slot
  {
    vkinterop::ExternalBuffer vk{};      ///< Vulkan-owned exportable buffer.
    void* vkCudaPtr{};                   ///< CUDA view of vk (imported).
    CudaInteropResourceHandle vkCudaHandle{}; ///< Bridge handle for cleanup.
    void* bouncePtr{};                   ///< Pinned CUDA bounce (cuMemAlloc).
  };

  CudaInteropContextHandle m_p2p{};
  vkinterop::VulkanCtx m_vk{};
  std::vector<Slot> m_slots;
  std::size_t m_frameBytes{};

  // Resolved once in init().
  void* m_fnCopyBuffer{};
  void* m_fnCopyBufferToImage{};
  void* m_fnPipelineBarrier{};

  // Timeline-semaphore fast path (replaces QRhi::finish() when supported).
  vkinterop::VkCudaTimelineSemaphore m_sem;
  uint64_t m_semValue{0};
  void* m_gfxQueue{};      ///< VkQueue from QRhiVulkanNativeHandles.
  void* m_fnQueueSubmit{}; ///< PFN_vkQueueSubmit.
};

} // namespace score::gfx::interop

#endif
