#pragma once

/**
 * @file VkHostImportUpload.hpp
 * @brief Zero-copy CPU->GPU upload on Vulkan via VK_EXT_external_memory_host.
 *
 * QRhi's own texture upload stages every frame through a buffer that VMA is
 * asked for with VMA_MEMORY_USAGE_CPU_TO_GPU (qrhivulkan.cpp). On a discrete
 * GPU that *prefers* DEVICE_LOCAL|HOST_VISIBLE, i.e. the small (~214-246 MB)
 * host-visible BAR heap. VMA treats a heap that size as "small", so its block
 * size is heapSize/8 and any allocation above half a block becomes a dedicated
 * vkAllocateMemory — and Qt destroys the staging buffer after every upload
 * ("no reuse of staging, this is intentional"). Past that threshold each frame
 * therefore pays a fresh BAR allocation: measured 13.4 MB on a 214 MB heap
 * (RTX 2080) and 15.4 MB on a 246 MB heap (RTX 4090), matching heapSize/16
 * exactly, with effective bandwidth falling from ~8 to ~2.4 GB/s (Windows) and
 * ~16 to ~9.9 GB/s (Linux).
 *
 * A capture slot has already been filled by the vendor SDK, so the right answer
 * is not a cheaper copy but no copy: import the slot's own pages as
 * VkDeviceMemory once at setup and let the GPU DMA straight out of them.
 * Measured 1.4x (SD/HD) to 6.4x (1440p, the worst Qt case) faster than
 * uploadTexture, and faster than every other backend's upload at 4K.
 *
 * Requirements: the imported pointer and the imported length must both be
 * multiples of minImportedHostPointerAlignment (4096 on NVIDIA), which is why
 * slots that want this path must be allocated through `alignedSlotAlloc`.
 */

#include <score_plugin_gfx_export.h>

#include <cstddef>
#include <cstdint>
#include <vector>

class QRhi;
class QRhiTexture;
class QRhiCommandBuffer;

namespace score::gfx::interop
{

/// Page-aligned slot storage. Plain new/vector alignment is not enough for
/// VK_EXT_external_memory_host.
SCORE_PLUGIN_GFX_EXPORT void* alignedSlotAlloc(std::size_t bytes, std::size_t alignment);
SCORE_PLUGIN_GFX_EXPORT void alignedSlotFree(void* p);

/// Storage a *graphics API* can import directly, as opposed to merely being
/// page-aligned.
///
/// On Windows the two are not the same thing:
/// ID3D12Device3::OpenExistingHeapFromAddress rejects `_aligned_malloc` memory
/// with E_INVALIDARG because it needs the base address of a virtual-memory
/// reservation at the 64 KB system allocation granularity, and a 4 KB-aligned
/// heap pointer lands mid-block (measured: 28672 % 65536). VirtualAlloc gives
/// that; Vulkan's VK_EXT_external_memory_host accepts it too, so one allocator
/// serves both rungs. Elsewhere this is just the page-aligned allocation.
///
/// Use for buffers a capture rung may hand to the GPU. Ordinary staging slots
/// should keep alignedSlotAlloc -- reserving 64 KB blocks for them would waste
/// address space for no benefit.
SCORE_PLUGIN_GFX_EXPORT void* importableAlloc(std::size_t bytes);
SCORE_PLUGIN_GFX_EXPORT void importableFree(void* p);

/// A host pointer imported as a Vulkan buffer. Handles are type-erased so
/// this header needs no Vulkan headers: buffer is a VkBuffer, memory a
/// VkDeviceMemory.
struct VkHostImportedBuffer
{
  void* buffer{};
  void* memory{};
  std::size_t importedBytes{};
};

/// Imports `host` as the storage of a new VkBuffer with the given
/// VkBufferUsageFlags. `host` must be VkHostImportUpload::requiredAlignment
/// aligned; the imported length is `bytes` rounded up to that alignment, so
/// the caller must own that much. With `requireHostCoherent`, only
/// HOST_COHERENT memory types are accepted (CPU reads then need no
/// vkInvalidateMappedMemoryRanges), preferring HOST_CACHED ones.
SCORE_PLUGIN_GFX_EXPORT bool importHostPointerBuffer(
    QRhi& rhi, void* host, std::size_t bytes, unsigned bufferUsage,
    bool requireHostCoherent, VkHostImportedBuffer& out);

/// GPU must be done with the buffer (e.g. after QRhi::finish()).
SCORE_PLUGIN_GFX_EXPORT void
releaseHostImportedBuffer(QRhi& rhi, VkHostImportedBuffer& buf);

class SCORE_PLUGIN_GFX_EXPORT VkHostImportUpload
{
public:
  ~VkHostImportUpload();

  /// Vulkan backend + VK_EXT_external_memory_host usable on this device.
  /// Returns 0 when unavailable, else the required pointer/length alignment.
  static std::size_t requiredAlignment(QRhi& rhi) noexcept;

  /// Imports each slot once. `slots` must be `alignment`-aligned and each at
  /// least `bytes` long; the imported length is `bytes` rounded up to the
  /// alignment, so the caller must have allocated that much.
  bool init(QRhi& rhi, const std::vector<void*>& slots, std::size_t bytes);
  void release();

  bool valid() const noexcept { return !m_buffers.empty(); }

  /// Records the slot -> texture DMA on `cb`. Must be called outside a render
  /// pass; leaves the texture in SHADER_READ_ONLY_OPTIMAL and tells QRhi so.
  /// @p srcOffset is the plane's byte offset inside the slot: a planar frame
  /// arrives as one contiguous buffer, so each plane is the same import at a
  /// different offset. Zero for single-plane formats.
  bool copyToTexture(
      QRhiCommandBuffer& cb, QRhiTexture& tex, std::size_t slot, int width,
      int height, std::size_t srcOffset = 0) noexcept;

private:
  struct Impl;
  Impl* m_d{};
  std::vector<void*> m_buffers; ///< VkBuffer per slot, type-erased
};

} // namespace score::gfx::interop
