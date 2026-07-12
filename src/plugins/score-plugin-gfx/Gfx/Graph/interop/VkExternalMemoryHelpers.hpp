#pragma once

/**
 * @file VkExternalMemoryHelpers.hpp
 * @brief Vulkan external-memory image/buffer create + export + import helpers.
 *
 * Collapses the boilerplate currently duplicated across:
 *   - `Gfx/Graph/decoders/HWCUDA.hpp` (HWCudaVulkanDecoder::setupPlane — export)
 *   - `Gfx/Spout/SpoutInput.cpp` (linkVulkanImage — import D3D11 KMT)
 *   - `Gfx/Spout/SpoutOutput.cpp` (linkVulkanImage — import D3D11 KMT)
 *   - future RDMA buffer-export code (export VkBuffer for CUDA P2P)
 *
 * Design notes:
 *   - Five primary functions: createExportableImage/Buffer, exportMemoryHandle,
 *     importExternalImage/Buffer. Plus a cleanup pair.
 *   - All `sType` fields are filled inside the .cpp; callers never see them.
 *   - PFN resolution (vkGetMemoryWin32HandlePropertiesKHR / vkGetMemoryFdKHR /
 *     etc.) is centralized — including the "use vkGetDeviceProcAddr, not
 *     vkGetInstanceProcAddr" gotcha documented at SpoutInput.cpp.
 *   - No QRhi knowledge — the helper deals only in raw Vulkan handles so the
 *     RDMA buffer case (which has no QRhiTexture) works alongside the
 *     decoder/Spout cases (which do `QRhiTexture::createFrom` afterwards).
 *   - Errors go to `qWarning()`; functions return empty optional on failure.
 */

#include <score/gfx/Vulkan.hpp>

#include <score_plugin_gfx_export.h>

#if QT_HAS_VULKAN

#include <vector>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <optional>

class QVulkanInstance;

namespace score::gfx::vkinterop
{

/**
 * @brief Platform-neutral OS handle for Vulkan external memory.
 *
 * On Linux, OPAQUE_FD / DMA_BUF use `fd >= 0`. On Windows, OPAQUE_WIN32,
 * D3D11_TEXTURE, D3D11_TEXTURE_KMT all use the same `HANDLE`. The `type`
 * field disambiguates which Vulkan import path the .cpp picks.
 */
struct ExternalHandle
{
#if defined(_WIN32)
  void* handle{nullptr}; // HANDLE
#else
  int fd{-1};
#endif
  VkExternalMemoryHandleTypeFlagBits type{};

  bool isValid() const noexcept
  {
#if defined(_WIN32)
    return handle != nullptr;
#else
    return fd >= 0;
#endif
  }

  // OS handle encoded as a void* for the CUDA interop layer / DVP, which expect
  // the Win32 HANDLE directly or the fd cast to pointer width (see
  // CudaInterop.cpp's CUDA_EXTERNAL_MEMORY_HANDLE_DESC fill-in).
  void* osHandle() const noexcept
  {
#if defined(_WIN32)
    return handle;
#else
    return reinterpret_cast<void*>(static_cast<intptr_t>(fd));
#endif
  }
};

// Platform-preferred opaque external-memory handle type: OPAQUE_WIN32 on
// Windows, OPAQUE_FD on Linux. Use this instead of hardcoding OPAQUE_FD so
// the same export/import code compiles and runs on both platforms.
inline constexpr VkExternalMemoryHandleTypeFlagBits kOpaqueHandleType =
#if defined(_WIN32)
    VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
    VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

/** @brief Description of an image to create with exportable / importable memory. */
struct ExternalImageDesc
{
  VkFormat format{};
  VkExtent3D extent{1, 1, 1};
  VkImageUsageFlags usage{};
  VkImageTiling tiling{VK_IMAGE_TILING_OPTIMAL};
  VkExternalMemoryHandleTypeFlagBits handleType{};
  // dedicated=true: required for D3D11_TEXTURE_KMT (Spout); harmless for OPAQUE_*.
  bool dedicated{true};
  bool preferDeviceLocal{true};
};

/** @brief Description of a buffer to create with exportable / importable memory. */
struct ExternalBufferDesc
{
  VkDeviceSize size{};
  VkBufferUsageFlags usage{};
  VkExternalMemoryHandleTypeFlagBits handleType{};
  bool dedicated{true};
};

/** @brief Vulkan handles the helpers need. Build once from QRhi native handles. */
struct VulkanCtx
{
  VkInstance instance{VK_NULL_HANDLE};
  VkPhysicalDevice physDev{VK_NULL_HANDLE};
  VkDevice dev{VK_NULL_HANDLE};
  QVulkanInstance* qInst{}; // for getInstanceProcAddr (PFN resolution)
};

/** Whether the live VkDevice was created WITH the timelineSemaphore feature
 *  (and the external-semaphore-fd extensions) enabled. Set by score's
 *  shared-device creation (Qt >= 6.6 path); stays false on QRhi-created
 *  devices, where using timeline semaphores is a spec violation even when
 *  the driver happens to tolerate it. Interop fast paths must gate on this. */
SCORE_PLUGIN_GFX_EXPORT void setDeviceTimelineSemaphoresEnabled(bool enabled);
SCORE_PLUGIN_GFX_EXPORT bool deviceTimelineSemaphoresEnabled();

struct ExternalImage
{
  VkImage image{VK_NULL_HANDLE};
  VkDeviceMemory memory{VK_NULL_HANDLE};
  VkDeviceSize size{0};
};

struct ExternalBuffer
{
  VkBuffer buffer{VK_NULL_HANDLE};
  VkDeviceMemory memory{VK_NULL_HANDLE};
  VkDeviceSize size{0};
};

// =============================================================================
// RDMA (VK_NV_external_memory_rdma) — GPU VRAM a third-party DMA engine
// (capture card with GPUDirect RDMA) can target via a remote address. Used by
// the RdmaGpuBuffer allocator. All functions return empty/nullopt when
// the extension is unavailable in the headers or at runtime (safe fallback).
// =============================================================================

struct RdmaBuffer
{
  VkBuffer buffer{VK_NULL_HANDLE};
  VkDeviceMemory memory{VK_NULL_HANDLE};
  VkDeviceSize size{0};
  std::uint64_t remoteAddress{0}; ///< VkRemoteAddressNV, for the card's SDK.
};

/// Allocate an RDMA-capable buffer (RDMA_CAPABLE memory type + RDMA_ADDRESS
/// export) and fetch its remote address. nullopt if VK_NV_external_memory_rdma
/// is unavailable.
SCORE_PLUGIN_GFX_EXPORT std::optional<RdmaBuffer>
createRdmaBuffer(const VulkanCtx&, VkDeviceSize size);

/// Fetch the RDMA remote address for RDMA-capable device memory.
/// nullopt if vkGetMemoryRemoteAddressNV is unavailable.
SCORE_PLUGIN_GFX_EXPORT std::optional<std::uint64_t>
getMemoryRemoteAddress(const VulkanCtx&, VkDeviceMemory);

SCORE_PLUGIN_GFX_EXPORT void destroyRdma(const VulkanCtx&, RdmaBuffer&);

// =============================================================================
// EXPORT — score allocates the resource, hands the handle to a peer.
// =============================================================================

/**
 * @brief Create a VkImage + exportable VkDeviceMemory, bound. No handle yet.
 *
 * Use exportMemoryHandle() to extract an fd / HANDLE afterwards.
 * Memory is DEVICE_LOCAL by default; falls back to any compatible type if
 * preferDeviceLocal is true but no DEVICE_LOCAL type matches the requirements.
 */
SCORE_PLUGIN_GFX_EXPORT std::optional<ExternalImage>
createExportableImage(const VulkanCtx&, const ExternalImageDesc&);

/**
 * @brief Create a VkBuffer + exportable VkDeviceMemory, bound. No handle yet.
 *
 * Symmetric with createExportableImage. Future use: RDMA buffer export.
 */
SCORE_PLUGIN_GFX_EXPORT std::optional<ExternalBuffer>
createExportableBuffer(const VulkanCtx&, const ExternalBufferDesc&);

/**
 * @brief Extract an OS handle (fd / HANDLE) from previously-allocated
 *        exportable memory.
 *
 * For OPAQUE_FD: ownership transfers to the caller per Vulkan spec — once
 * imported into CUDA (or the peer device), the fd should not be closed
 * separately. For OPAQUE_WIN32 / D3D11_*: caller owns the HANDLE and must
 * CloseHandle() when done (CUDA does not take ownership).
 */
SCORE_PLUGIN_GFX_EXPORT std::optional<ExternalHandle> exportMemoryHandle(
    const VulkanCtx&, VkDeviceMemory, VkExternalMemoryHandleTypeFlagBits);

// =============================================================================
// IMPORT — score consumes a handle produced by a peer (Spout, CUDA, etc).
// =============================================================================

/**
 * @brief Create a VkImage backed by memory imported from an external handle.
 *
 * Collapses SpoutInput::linkVulkanImage and SpoutOutput::linkVulkanImage.
 * Internally:
 *   1. capability probe via vkGetPhysicalDeviceImageFormatProperties2
 *   2. memory-type intersection via vkGetMemoryWin32HandlePropertiesKHR /
 *      vkGetMemoryFdPropertiesKHR (the call SpoutOutput omitted, fixing a
 *      latent compat bug)
 *   3. VkImportMemoryWin32HandleInfoKHR / VkImportMemoryFdInfoKHR
 *   4. VkMemoryDedicatedAllocateInfo (when desc.dedicated)
 *   5. vkAllocateMemory + vkBindImageMemory
 */
SCORE_PLUGIN_GFX_EXPORT std::optional<ExternalImage> importExternalImage(
    const VulkanCtx&, const ExternalImageDesc&, const ExternalHandle&);

/**
 * @brief Create a VkBuffer backed by memory imported from an external handle.
 */
SCORE_PLUGIN_GFX_EXPORT std::optional<ExternalBuffer> importExternalBuffer(
    const VulkanCtx&, const ExternalBufferDesc&, const ExternalHandle&);

// =============================================================================
// Cleanup — symmetric helpers. Out-of-line because they need to dispatch
// through QVulkanInstance::deviceFunctions(), and we don't want to drag
// <QVulkanDeviceFunctions> into every consumer of this header.
// =============================================================================

SCORE_PLUGIN_GFX_EXPORT void destroyExternal(const VulkanCtx& v, ExternalImage& img);
SCORE_PLUGIN_GFX_EXPORT void destroyExternal(const VulkanCtx& v, ExternalBuffer& buf);

// =============================================================================
// Layout transition — one-time image layout change via a transient command
// buffer (create pool + buffer, record a VkImageMemoryBarrier, submit, wait
// idle). Used by the RDMA capture/output paths to move a freshly-created
// exportable VkImage UNDEFINED -> GENERAL once, so the QRhi-adopted texture
// (told GENERAL via createFrom) and CUDA (which reads/writes the memory) agree.
//
// Broad, conservative access/stage masks cover both the capture (shader-read)
// and output (transfer-write) uses; the following vkQueueWaitIdle makes the
// choice of masks immaterial to correctness for a one-time init transition.
// @p queue / @p queueFamily are the graphics queue + family the barrier submits
// on (from QRhiVulkanNativeHandles). Returns false on any Vulkan failure.
// =============================================================================
SCORE_PLUGIN_GFX_EXPORT bool transitionImageLayout(
    const VulkanCtx& v, VkQueue queue, std::uint32_t queueFamily, VkImage image,
    VkImageLayout oldLayout, VkImageLayout newLayout);

// =============================================================================
// DMA-BUF modifier capability probes
// =============================================================================

/**
 * @brief Query whether the GPU can import a DMA-BUF for (format, modifier,
 *        extent) via VK_EXT_image_drm_format_modifier.
 *
 * Used during PipeWire EnumFormat construction: we only advertise modifiers
 * we can actually import, so producers can fixate on a supported choice.
 *
 * Bypasses the call if VK_EXT_image_drm_format_modifier isn't loaded —
 * returns false. The shared layer's format_negotiation falls back to LINEAR
 * (always supported) when this returns false.
 */
bool can_import_dmabuf_modifier(
    const VulkanCtx& v, VkFormat format, std::uint64_t drm_modifier,
    std::uint32_t width, std::uint32_t height) noexcept;

/**
 * @brief Enumerate the DMA-BUF modifiers the GPU can import for a given
 *        VkFormat. Returns at minimum DRM_FORMAT_MOD_LINEAR (0); empty if
 *        VK_EXT_image_drm_format_modifier unavailable.
 *
 * Used by the producer to populate the modifier choice list in EnumFormat.
 */
std::vector<std::uint64_t> supported_dmabuf_modifiers(
    const VulkanCtx& v, VkFormat format) noexcept;

} // namespace score::gfx::vkinterop

#endif // QT_HAS_VULKAN
