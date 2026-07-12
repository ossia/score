#pragma once

#if defined(__linux__)
#include <score/gfx/Vulkan.hpp>

#if QT_HAS_VULKAN
#include <QtGui/private/qrhivulkan_p.h>
#include <qvulkanfunctions.h>
#include <vulkan/vulkan.h>

#include <cstring>
#include <vector>

#include <unistd.h>

#if defined(VK_EXT_image_drm_format_modifier) && defined(VK_KHR_external_memory_fd) \
    && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)

namespace score::gfx
{

/**
 * @brief Shared utility for importing DMA-BUF file descriptors into Vulkan.
 *
 * Used by both HWVaapiVulkanDecoder and HWVulkanDecoder to import
 * per-plane DMA-BUF fds as VkImages backed by external memory.
 */
struct DMABufPlaneImporter
{
  VkDevice m_dev{VK_NULL_HANDLE};
  QVulkanDeviceFunctions* m_dfuncs{};
  PFN_vkGetMemoryFdPropertiesKHR m_getMemFdProps{};

  struct PlaneImport
  {
    VkImage image{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
  };

  void init(QRhi& rhi)
  {
    auto* nh
        = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());
    m_dev = nh->dev;
    m_dfuncs = nh->inst->deviceFunctions(m_dev);
    m_getMemFdProps = reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(
        nh->inst->getInstanceProcAddr("vkGetMemoryFdPropertiesKHR"));
  }

  static bool isAvailable(QRhi& rhi)
  {
    if(rhi.backend() != QRhi::Vulkan)
      return false;
    auto* nh
        = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());
    if(!nh || !nh->dev || !nh->physDev || !nh->inst)
      return false;
    if(!nh->inst->getInstanceProcAddr("vkGetMemoryFdPropertiesKHR"))
      return false;

    // Verify VK_EXT_external_memory_dma_buf is available on the device.
    // Without it, DMA-BUF import (VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT)
    // will fail. This also catches multi-GPU scenarios where VAAPI decodes on
    // Intel but Vulkan renders on NVIDIA (which may not support DMA-BUF import).
    auto* funcs = nh->inst->functions();
    uint32_t extCount = 0;
    funcs->vkEnumerateDeviceExtensionProperties(
        nh->physDev, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> exts(extCount);
    funcs->vkEnumerateDeviceExtensionProperties(
        nh->physDev, nullptr, &extCount, exts.data());

    bool hasDmaBuf = false;
    bool hasDrmModifier = false;
    for(auto& e : exts)
    {
#ifdef VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME
      if(std::strcmp(e.extensionName, VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME) == 0)
        hasDmaBuf = true;
#endif
#ifdef VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME
      if(std::strcmp(e.extensionName, VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME) == 0)
        hasDrmModifier = true;
#endif
    }
    return hasDmaBuf && hasDrmModifier;
  }

  void cleanupPlane(PlaneImport& p)
  {
    if(p.image != VK_NULL_HANDLE)
      m_dfuncs->vkDestroyImage(m_dev, p.image, nullptr);
    if(p.memory != VK_NULL_HANDLE)
      m_dfuncs->vkFreeMemory(m_dev, p.memory, nullptr);
    p = {};
  }

  bool importPlane(
      PlaneImport& out, int fd, uint64_t modifier, ptrdiff_t offset,
      ptrdiff_t pitch, VkFormat format, int w, int h)
  {
    // --- VkImage creation with external memory + DRM format modifier ---

    VkSubresourceLayout planeLayout{};
    planeLayout.offset = static_cast<VkDeviceSize>(offset);
    planeLayout.rowPitch = static_cast<VkDeviceSize>(pitch);

    VkImageDrmFormatModifierExplicitCreateInfoEXT modInfo{};
    modInfo.sType
        = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT;
    modInfo.drmFormatModifier = modifier;
    modInfo.drmFormatModifierPlaneCount = 1;
    modInfo.pPlaneLayouts = &planeLayout;

    VkExternalMemoryImageCreateInfo extInfo{};
    extInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    extInfo.pNext = &modInfo;
    extInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.pNext = &extInfo;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = format;
    imgInfo.extent
        = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    imgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage image{};
    if(m_dfuncs->vkCreateImage(m_dev, &imgInfo, nullptr, &image)
       != VK_SUCCESS)
      return false;

    // --- Memory import from DMA-BUF fd ---

    VkMemoryRequirements memReqs{};
    m_dfuncs->vkGetImageMemoryRequirements(m_dev, image, &memReqs);

    int dupFd = dup(fd);
    if(dupFd < 0)
    {
      m_dfuncs->vkDestroyImage(m_dev, image, nullptr);
      return false;
    }

    VkMemoryFdPropertiesKHR fdProps{};
    fdProps.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
    if(m_getMemFdProps(
           m_dev, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, dupFd,
           &fdProps)
       != VK_SUCCESS)
    {
      close(dupFd);
      m_dfuncs->vkDestroyImage(m_dev, image, nullptr);
      return false;
    }

    uint32_t memTypeIdx = UINT32_MAX;
    uint32_t compatible = memReqs.memoryTypeBits & fdProps.memoryTypeBits;
    for(uint32_t i = 0; i < 32; ++i)
    {
      if(compatible & (1u << i))
      {
        memTypeIdx = i;
        break;
      }
    }
    if(memTypeIdx == UINT32_MAX)
    {
      close(dupFd);
      m_dfuncs->vkDestroyImage(m_dev, image, nullptr);
      return false;
    }

    VkImportMemoryFdInfoKHR importInfo{};
    importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
    importInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    importInfo.fd = dupFd;

    VkMemoryDedicatedAllocateInfo dedicatedInfo{};
    dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedInfo.pNext = &importInfo;
    dedicatedInfo.image = image;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = &dedicatedInfo;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memTypeIdx;

    VkDeviceMemory memory{};
    if(m_dfuncs->vkAllocateMemory(m_dev, &allocInfo, nullptr, &memory)
       != VK_SUCCESS)
    {
      close(dupFd);
      m_dfuncs->vkDestroyImage(m_dev, image, nullptr);
      return false;
    }

    if(m_dfuncs->vkBindImageMemory(m_dev, image, memory, 0) != VK_SUCCESS)
    {
      m_dfuncs->vkFreeMemory(m_dev, memory, nullptr);
      m_dfuncs->vkDestroyImage(m_dev, image, nullptr);
      return false;
    }

    out.image = image;
    out.memory = memory;
    return true;
  }
};

} // namespace score::gfx

#endif // VK_EXT_image_drm_format_modifier && VK_KHR_external_memory_fd
#endif // QT_HAS_VULKAN
#endif // __linux__
