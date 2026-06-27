#pragma once

/**
 * @file VkExternalMemoryHelpers.hpp
 * @brief Vulkan external-memory image/buffer create + export + import helpers.
 *
 * Collapses the boilerplate currently duplicated across:
 *   - `Gfx/Graph/decoders/HWCUDA.hpp` (HWCudaVulkanDecoder::setupPlane — export)
 *   - `Gfx/Spout/SpoutInput.cpp` (linkVulkanImage — import D3D11 KMT)
 *   - `Gfx/Spout/SpoutOutput.cpp` (linkVulkanImage — import D3D11 KMT)
 *   - future `RdmaInteropVulkanTier3.cpp` (export VkBuffer for CUDA P2P)
 *
 * Design notes:
 *   - Five primary functions: createExportableImage/Buffer, exportMemoryHandle,
 *     importExternalImage/Buffer. Plus a cleanup pair.
 *   - All `sType` fields are filled inside the .cpp; callers never see them.
 *   - PFN resolution (vkGetMemoryWin32HandlePropertiesKHR / vkGetMemoryFdKHR /
 *     etc.) is centralized — including the "use vkGetDeviceProcAddr, not
 *     vkGetInstanceProcAddr" gotcha documented at SpoutInput.cpp.
 *   - No QRhi knowledge — the helper deals only in raw Vulkan handles so the
 *     RDMA tier-3 buffer case (which has no QRhiTexture) works alongside the
 *     decoder/Spout cases (which do `QRhiTexture::createFrom` afterwards).
 *   - Errors go to `qWarning()`; functions return empty optional on failure.
 */

#include <score/gfx/Vulkan.hpp>

#include <score_plugin_gfx_export.h>

#if QT_HAS_VULKAN

#include <vector>

#include <vulkan/vulkan.h>

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
};

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
 * Symmetric with createExportableImage. Future use: RdmaInteropVulkanTier3.
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
// DMA-BUF modifier capability probes (Phase 5)
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
