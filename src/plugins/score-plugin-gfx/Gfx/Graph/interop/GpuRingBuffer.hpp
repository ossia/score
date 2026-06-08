#pragma once

/**
 * @file GpuRingBuffer.hpp
 * @brief Backend-uniform N-slot exportable GPU buffer ring for vendor P2P.
 *
 * Captures the QRhi-adopted-buffer ring pattern shared by every "tier-3"
 * GPU-direct video output strategy:
 *
 *   1. Allocate N QRhi StorageBuffers.
 *   2. Extract each underlying native buffer (ID3D11Buffer*, GLuint, ...)
 *      via `QRhiBuffer::nativeBuffer()`.
 *   3. Import each into CUDA via the `CudaP2PBridge` C API.
 *   4. Hand the resulting flat GPU device pointer to the peer device's
 *      DMA-pin API (AJA `DMABufferLock(inRDMA=true)`, Magewell
 *      `MWPinVideoBuffer`, Rivermax `rmx_register_memory`, ...).
 *   5. Per frame: rotate the ring; encoder writes into the current slot's
 *      QRhi buffer; peer device DMA-reads from the same slot's GPU
 *      device pointer.
 *
 * Replaces the open-coded slot ring in `AJA/RdmaInteropD3D11Tier3.hpp` and
 * `AJA/RdmaInteropGLTier3.hpp` so that future addons (Magewell, Rivermax)
 * don't have to re-implement the dance. Vendor-specific concerns (the
 * `DMABufferLock` call, the per-frame submission API, pacing) stay in
 * each vendor's addon.
 *
 * Backend support matrix:
 *   - D3D11   : real (ID3D11Buffer + cuda_p2p_import_d3d11_buffer)
 *   - OpenGL  : real (GLuint + cuda_p2p_import_gl_buffer)
 *   - Vulkan  : stub (QRhi VMA-allocates without VkExportMemoryAllocateInfo;
 *               needs either a QRhi private-API patch or a parallel
 *               externally-managed buffer ring with beginExternal compute
 *               dispatch)
 *   - D3D12   : stub (QRhi D3D12 backend lacks SHARED heap support)
 */

#include <Gfx/Graph/interop/CudaP2PBridge.h>
#include <score_plugin_gfx_export.h>

#include <QtGui/private/qrhi_p.h>

#include <cstdint>
#include <vector>

namespace score::gfx::interop
{

/**
 * @brief One slot of the ring. Lifetime owned by GpuRingBuffer.
 */
struct GpuRingBufferSlot
{
  QRhiBuffer* qrhiBuffer{};            ///< QRhi-owned UAV / Storage buffer.
  CudaP2PResourceHandle cudaHandle{};  ///< Bridge handle for cleanup.
  void* gpuDevicePtr{};                ///< Flat GPU device pointer the peer DMA's.
};

struct GpuRingBufferConfig
{
  QRhi* rhi{};
  CudaP2PContextHandle cudaCtx{};      ///< Caller-owned, must be inited.
  std::uint32_t bufferSize{};
  int slotCount{2};
  const char* debugName{"score-gpu-ring"};
};

/**
 * @brief Vendor-neutral helper that allocates + manages the slot ring.
 *
 * Usage (per-vendor adapter):
 * @code
 *   CudaP2PContextHandle cuda{};
 *   cuda_p2p_init(&cuda);
 *
 *   GpuRingBuffer ring;
 *   if(!ring.create({rhi, cuda, frameByteSize, 2, "MyVendor-RDMA"}))
 *     return false;
 *
 *   // 1× per slot: register the GPU pointer with the vendor's DMA-pin API.
 *   for(std::size_t i = 0; i < ring.slotCount(); ++i)
 *     vendor->RegisterGpuBuffer(ring.slot(i).gpuDevicePtr, frameByteSize);
 *
 *   // Per frame: encoder writes into ring.slot(ring.writeIndex()).qrhiBuffer;
 *   //            then ring.advance() rotates; peer DMAs from the just-written slot.
 * @endcode
 */
class SCORE_PLUGIN_GFX_EXPORT GpuRingBuffer
{
public:
  GpuRingBuffer() = default;
  ~GpuRingBuffer();

  GpuRingBuffer(const GpuRingBuffer&) = delete;
  GpuRingBuffer& operator=(const GpuRingBuffer&) = delete;
  GpuRingBuffer(GpuRingBuffer&&) = delete;
  GpuRingBuffer& operator=(GpuRingBuffer&&) = delete;

  /**
   * @brief Allocate slots + CUDA-import each. Backend dispatch happens
   *        internally based on rhi->backend().
   *
   * Returns false on any failure; partial state is released before
   * returning so the caller doesn't need to call destroy().
   */
  bool create(const GpuRingBufferConfig& cfg);

  /// Destroy all slots + CUDA mappings. Safe to call multiple times.
  void destroy();

  bool valid() const noexcept { return !m_slots.empty(); }
  std::size_t slotCount() const noexcept { return m_slots.size(); }

  GpuRingBufferSlot& slot(std::size_t i) noexcept { return m_slots[i]; }
  const GpuRingBufferSlot& slot(std::size_t i) const noexcept { return m_slots[i]; }

  std::size_t writeIndex() const noexcept { return m_writeIndex; }
  /// Rotate the ring; returns the new write index.
  std::size_t advance() noexcept;

private:
  GpuRingBufferConfig m_cfg{};
  std::vector<GpuRingBufferSlot> m_slots;
  std::size_t m_writeIndex{};

  bool createD3D11();
  bool createOpenGL();
  bool createVulkanStub();
  bool createD3D12Stub();
};

} // namespace score::gfx::interop
