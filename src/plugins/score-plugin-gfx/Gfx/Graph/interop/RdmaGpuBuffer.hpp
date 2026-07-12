#pragma once

/**
 * @file RdmaGpuBuffer.hpp
 * @brief Vendor-neutral allocator of RDMA-capable GPU VRAM.
 *
 * The counterpart to HostPinnedRing for true GPU-direct access: where
 * HostPinnedRing hands a card a page-locked *host* buffer, RdmaGpuBuffer hands
 * it a slot of GPU *VRAM* the card can DMA into/out of directly (GPUDirect
 * RDMA). Each slot exposes the per-API handoff the card's SDK wants:
 *
 *   - CUDA   : a CUdeviceptr (BAR1-mappable, gpuDirectRDMACapable) — reuses
 *              CudaVmmAllocator.
 *   - Vulkan : the VkRemoteAddressNV from VK_NV_external_memory_rdma — reuses
 *              vkinterop::createRdmaBuffer.
 *   - D3D12  : the GPU VA from NvAPI_D3D12_CreateCommittedRDMABuffer — gated on
 *              SCORE_HAS_NVAPI (off unless an NvAPI SDK is configured).
 *   - Host   : a 64 KiB-aligned host pointer — the non-RDMA fallback (the card's
 *              app-buffer path with RDMA disabled).
 *
 * This is the allocator only. Wrapping a slot as a QRhi resource for the
 * encoder/decoder to render into/sample from is the consumer's concern (the
 * vendor backend, e.g. Deltacast). The first consumer is Deltacast's
 * VHD_CreateSlotEx(RDMAEnabled=TRUE), which takes exactly {pBuffer=gpuVA, ctx}.
 */

#include <score_plugin_gfx_export.h>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace score::gfx
{
struct CudaFunctions;
}

namespace score::gfx::interop
{

/// Which allocation mechanism a RdmaGpuBuffer instance uses.
enum class RdmaGpuApi : std::uint8_t
{
  None = 0,
  Cuda,         ///< CUDA VMM (CUdeviceptr); reuses CudaVmmAllocator.
  Vulkan,       ///< VK_NV_external_memory_rdma (remote address).
  D3D12,        ///< NvAPI CreateCommittedRDMABuffer (GPU VA). SCORE_HAS_NVAPI.
  HostFallback, ///< 64 KiB-aligned host memory (non-RDMA app-buffer path).
};

/// One allocated slot. `gpuVA` is the per-API handoff the card's SDK wants.
struct RdmaGpuSlot
{
  void* gpuVA{};       ///< CUdeviceptr / VkRemoteAddressNV / D3D12 VA / host ptr.
  std::size_t size{};  ///< Actual size (granularity / 64 KiB rounded).
  void* opaque{};      ///< Backend-owned per-slot context (freed in destroy()).
};

struct RdmaGpuBufferConfig
{
  RdmaGpuApi api{RdmaGpuApi::None};
  std::uint32_t slotCount{};
  std::size_t frameBytes{};

  // Backend context — only the field matching `api` is read; the caller owns
  // the lifetime of whatever it points to.
  score::gfx::CudaFunctions* cuda{}; ///< Cuda: loaded fns; CUDA ctx must be
                                     ///< current on the calling thread.
  int cudaDevice{0};                 ///< Cuda: device ordinal.
  const void* vulkanCtx{};           ///< Vulkan: const vkinterop::VulkanCtx*.
  void* d3d12Device{};               ///< D3D12: ID3D12Device*.
};

/**
 * @brief Owns N RDMA-capable GPU buffers. Move-only; destroy() is idempotent.
 */
class SCORE_PLUGIN_GFX_EXPORT RdmaGpuBuffer
{
public:
  RdmaGpuBuffer() noexcept;
  ~RdmaGpuBuffer();

  RdmaGpuBuffer(RdmaGpuBuffer&&) noexcept;
  RdmaGpuBuffer& operator=(RdmaGpuBuffer&&) noexcept;
  RdmaGpuBuffer(const RdmaGpuBuffer&) = delete;
  RdmaGpuBuffer& operator=(const RdmaGpuBuffer&) = delete;

  /// Allocate `slotCount` buffers of `frameBytes` each via `api`. On any failure
  /// partial state is released and false returned.
  bool create(const RdmaGpuBufferConfig& cfg);

  bool valid() const noexcept;
  RdmaGpuApi backend() const noexcept;
  std::size_t slotCount() const noexcept;
  const RdmaGpuSlot& slot(std::size_t i) const noexcept;

  /// Release every slot's backend resource. Idempotent.
  void destroy() noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace score::gfx::interop
