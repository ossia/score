#pragma once

/**
 * @file VkCudaSemaphore.hpp
 * @brief Exportable Vulkan timeline semaphore + CUDA-side import pair.
 *
 * The canonical sync primitive for the tier-0 capture flows that span
 * the Vulkan and CUDA worlds:
 *
 *   - Rivermax: NIC writes into a CUDA-allocated VRAM region; the
 *     score render graph then samples that region from a Vulkan-
 *     imported QRhiTexture. The Vulkan side needs to wait for the
 *     CUDA-stream signalling that the NIC has finished. Inversely,
 *     score may signal a Vulkan semaphore after rendering completes,
 *     and the CUDA stream waits before reusing the buffer.
 *
 *   - AJA-RDMA via Vulkan QRhi: same shape — AJA's `WaitForOutput`
 *     completion sets a CUDA event; Vulkan must wait.
 *
 * The class wraps the two-step setup:
 *   1. Create a Vulkan exportable VK_SEMAPHORE_TYPE_TIMELINE semaphore
 *      with VkExportSemaphoreCreateInfo + the OS-appropriate handle
 *      type (OPAQUE_FD on Linux, OPAQUE_WIN32 on Windows).
 *   2. Export an FD / HANDLE via vkGetSemaphoreFdKHR /
 *      vkGetSemaphoreWin32HandleKHR.
 *   3. cuImportExternalSemaphore on the CUDA side — reuses the
 *      existing `CudaP2PBridge::cuda_p2p_import_vulkan_semaphore`
 *      shim, so no new CUDA-side code is needed.
 *
 * This file owns the Vulkan-side creation; the CUDA-side handle stays
 * with `CudaP2PBridge` so consumers can call `cuda_p2p_wait_semaphore`
 * / `cuda_p2p_signal_semaphore` against it.
 */

#include <Gfx/Graph/interop/CudaP2PBridge.h>
#include <Gfx/Graph/interop/VkExternalMemoryHelpers.hpp>

#include <score_plugin_gfx_export.h>

#include <cstdint>

namespace score::gfx::vkinterop
{

/** A live exportable timeline semaphore on the Vulkan side, paired
 *  with a CUDA-imported handle on the CUDA side. Move-only. */
class SCORE_PLUGIN_GFX_EXPORT VkCudaTimelineSemaphore
{
public:
  VkCudaTimelineSemaphore() = default;
  ~VkCudaTimelineSemaphore();

  VkCudaTimelineSemaphore(const VkCudaTimelineSemaphore&) = delete;
  VkCudaTimelineSemaphore& operator=(const VkCudaTimelineSemaphore&) = delete;
  VkCudaTimelineSemaphore(VkCudaTimelineSemaphore&&) noexcept;
  VkCudaTimelineSemaphore& operator=(VkCudaTimelineSemaphore&&) noexcept;

  /** Create both the Vulkan semaphore and its CUDA import.
   *
   *  @param vk         Vulkan device handles.
   *  @param cudaCtx    Pre-initialised CudaP2PContextHandle (the CUDA
   *                    context that will wait/signal on this).
   *  @param initialValue  Initial counter value for the timeline
   *                       semaphore (typically 0).
   *  @return true on success. On failure both sides are torn down. */
  bool create(const VulkanCtx& vk, CudaP2PContextHandle cudaCtx,
              uint64_t initialValue = 0);

  /** Tear down both sides. Idempotent. */
  void destroy() noexcept;

  bool valid() const noexcept;

  /** The raw Vulkan semaphore — pass to vkQueueSubmit's signal/wait
   *  vector via VkTimelineSemaphoreSubmitInfo. */
  VkSemaphore vk() const noexcept { return m_vkSem; }

  /** The CUDA-side handle. Pass to `cuda_p2p_wait_semaphore` /
   *  `cuda_p2p_signal_semaphore`. */
  CudaP2PSemaphoreHandle cuda() const noexcept { return m_cudaSem; }

  /** Signal the timeline counter from the CPU side to `value`.
   *  Useful for tearing down a hung waiter at shutdown.
   *  Returns true on success. */
  bool hostSignal(uint64_t value);

private:
  VulkanCtx m_vk{};
  CudaP2PContextHandle m_cudaCtx{};
  VkSemaphore m_vkSem{VK_NULL_HANDLE};
  CudaP2PSemaphoreHandle m_cudaSem{};
};

} // namespace score::gfx::vkinterop
