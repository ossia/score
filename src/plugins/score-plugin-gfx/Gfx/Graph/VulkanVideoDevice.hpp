#pragma once
#include <score/gfx/Vulkan.hpp>

#if QT_HAS_VULKAN && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)

#include <QtGui/private/qrhivulkan_p.h>
#include <qvulkanfunctions.h>
#include <vulkan/vulkan.h>

#include <cstring>
#include <string>
#include <vector>

#ifdef VK_KHR_video_decode_queue

namespace score::gfx
{

/**
 * @brief Shared Vulkan device info for FFmpeg + QRhi interop.
 *
 * When Vulkan Video decode is available, we create the VkDevice ourselves
 * with both graphics and video decode queue families enabled, then import
 * it into QRhi. This allows FFmpeg and QRhi to share the same device,
 * enabling zero-copy texture wrapping of decoded AVVkFrame VkImages.
 */
struct SharedVulkanDevice
{
  VkPhysicalDevice physDev{VK_NULL_HANDLE};
  VkDevice dev{VK_NULL_HANDLE};
  VkQueue gfxQueue{VK_NULL_HANDLE};
  uint32_t gfxQueueFamilyIdx{0};
  bool hasVideoDecodeQueue{false};
  uint32_t videoDecodeQueueFamilyIdx{0};

  // Persistent storage for extension name strings (FFmpeg needs const char*)
  std::vector<std::string> enabledExtensions;

  // Queue family info for FFmpeg's AVVulkanDeviceContext
  struct QueueFamilyInfo
  {
    uint32_t idx;
    uint32_t count;
    VkQueueFlags flags;
  };
  std::vector<QueueFamilyInfo> queueFamilies;

  void destroy()
  {
    if(dev != VK_NULL_HANDLE)
    {
      auto* inst = staticVulkanInstance(false);
      if(inst)
      {
        auto fn = reinterpret_cast<PFN_vkDestroyDevice>(
            inst->getInstanceProcAddr("vkDestroyDevice"));
        if(fn)
          fn(dev, nullptr);
      }
      dev = VK_NULL_HANDLE;
    }
  }

  explicit operator bool() const { return dev != VK_NULL_HANDLE; }
};

/**
 * @brief Returns the curated list of Vulkan device extensions needed
 *        for Qt QRhi + FFmpeg Vulkan Video + our zero-copy decoders.
 *
 * Used by both createSharedVulkanDevice() and setupHardwareDecoder()
 * to ensure consistent extension reporting.
 */
inline std::vector<const char*> sharedVulkanDeviceExtensions()
{
  return {
      // Qt QRhi requirements
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef VK_KHR_MAINTENANCE_1_EXTENSION_NAME
      VK_KHR_MAINTENANCE_1_EXTENSION_NAME,
#endif
#ifdef VK_KHR_MAINTENANCE_2_EXTENSION_NAME
      VK_KHR_MAINTENANCE_2_EXTENSION_NAME,
#endif
#ifdef VK_KHR_MAINTENANCE_3_EXTENSION_NAME
      VK_KHR_MAINTENANCE_3_EXTENSION_NAME,
#endif
#ifdef VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME
      VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME,
#endif
#ifdef VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME
      VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
#endif
#ifdef VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME
      VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
#endif
#ifdef VK_KHR_MULTIVIEW_EXTENSION_NAME
      VK_KHR_MULTIVIEW_EXTENSION_NAME,
#endif
#ifdef VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME
      VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
#endif
#ifdef VK_KHR_SPIRV_1_4_EXTENSION_NAME
      VK_KHR_SPIRV_1_4_EXTENSION_NAME,
#endif
#ifdef VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME
      VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME,
#endif
      // External memory (DMA-BUF, CUDA interop)
      VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
#ifdef VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME
      VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
#endif
#ifdef VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME
      VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
#endif
#ifdef VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME
      VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
#endif
#ifdef VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME
      VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
#endif
#ifdef VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME
      VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
#endif
#ifdef VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME
      VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
#endif
#ifdef VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME
      VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME,
#endif
      VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
      VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
      // Vulkan Video decode
      VK_KHR_VIDEO_QUEUE_EXTENSION_NAME,
      VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME,
#ifdef VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME
      VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME,
#endif
#ifdef VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME
      VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME,
#endif
#ifdef VK_KHR_VIDEO_DECODE_AV1_EXTENSION_NAME
      VK_KHR_VIDEO_DECODE_AV1_EXTENSION_NAME,
#endif
      // Synchronization
      VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
      VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
      // YCbCr
#ifdef VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME
      VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
#endif
#ifdef VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
      VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
#endif
#ifdef VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
      VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
#endif
#ifdef VK_KHR_FORMAT_FEATURE_FLAGS_2_EXTENSION_NAME
      VK_KHR_FORMAT_FEATURE_FLAGS_2_EXTENSION_NAME,
#endif
#ifdef VK_KHR_COPY_COMMANDS_2_EXTENSION_NAME
      VK_KHR_COPY_COMMANDS_2_EXTENSION_NAME,
#endif
#ifdef VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME
      VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME,
#endif
#ifdef VK_KHR_VIDEO_MAINTENANCE_1_EXTENSION_NAME
      VK_KHR_VIDEO_MAINTENANCE_1_EXTENSION_NAME,
#endif
#ifdef VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME
      VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
#endif
#ifdef VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
      VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
#endif
  };
}

/**
 * @brief Create a VkDevice with video decode queue support.
 *
 * Creates a VkDevice with queues from ALL available queue families
 * (graphics, compute, transfer, video decode, etc.) and enables
 * ALL available device extensions and features. This ensures
 * compatibility with both Qt's QRhi and FFmpeg's Vulkan hwcontext.
 *
 * Returns empty SharedVulkanDevice if video decode is not available
 * or device creation fails. In that case, caller should fall back
 * to normal QRhi device creation.
 */
/// If preferredPhysDev is non-null, use that GPU. Otherwise use the first one.
inline SharedVulkanDevice createSharedVulkanDevice(
    QVulkanInstance* inst, VkPhysicalDevice preferredPhysDev = VK_NULL_HANDLE)
{
  SharedVulkanDevice result;
  if(!inst)
    return result;

  auto* funcs = inst->functions();
  if(!funcs)
    return result;

  // --- Pick physical device (prefer discrete GPU) ---

  uint32_t devCount = 0;
  funcs->vkEnumeratePhysicalDevices(inst->vkInstance(), &devCount, nullptr);
  if(devCount == 0)
    return result;

  std::vector<VkPhysicalDevice> physDevs(devCount);
  funcs->vkEnumeratePhysicalDevices(inst->vkInstance(), &devCount, physDevs.data());

  // Use the caller-specified physical device (matching QRhi's GPU),
  // or fall back to the first one.
  if(preferredPhysDev != VK_NULL_HANDLE)
    result.physDev = preferredPhysDev;
  else
    result.physDev = physDevs[0];

  uint32_t qfCount = 0;
  funcs->vkGetPhysicalDeviceQueueFamilyProperties(
      result.physDev, &qfCount, nullptr);
  if(qfCount == 0)
    return result;

  std::vector<VkQueueFamilyProperties> qfProps(qfCount);
  funcs->vkGetPhysicalDeviceQueueFamilyProperties(
      result.physDev, &qfCount, qfProps.data());

  bool foundGfx = false;
  for(uint32_t i = 0; i < qfCount; i++)
  {
    if(!foundGfx && (qfProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
    {
      result.gfxQueueFamilyIdx = i;
      foundGfx = true;
    }
    if(qfProps[i].queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR)
    {
      result.hasVideoDecodeQueue = true;
      result.videoDecodeQueueFamilyIdx = i;
    }
  }

  if(!foundGfx || !result.hasVideoDecodeQueue)
    return {};

  // --- Enumerate available device extensions ---

  uint32_t extCount = 0;
  funcs->vkEnumerateDeviceExtensionProperties(
      result.physDev, nullptr, &extCount, nullptr);
  std::vector<VkExtensionProperties> avail(extCount);
  funcs->vkEnumerateDeviceExtensionProperties(
      result.physDev, nullptr, &extCount, avail.data());

  auto hasExt = [&](const char* name) {
    for(auto& e : avail)
      if(std::strcmp(e.extensionName, name) == 0)
        return true;
    return false;
  };

  // Only enable extensions that are actually available from the curated list.
  // Use string literal pointers directly — DO NOT copy into std::string
  // then take c_str(), as vector<string> reallocation invalidates all
  // previous c_str() pointers (caused vkCreateDevice to get garbage names).
  auto wantedExtensions = sharedVulkanDeviceExtensions();
  std::vector<const char*> extPtrs;
  for(auto* ext : wantedExtensions)
  {
    if(hasExt(ext))
    {
      result.enabledExtensions.push_back(ext);
      extPtrs.push_back(ext); // ext is a string literal, always valid
    }
  }

  // --- Query and enable ALL supported features ---

  auto vkGetPhysicalDeviceFeatures2Fn
      = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
          inst->getInstanceProcAddr("vkGetPhysicalDeviceFeatures2"));
  if(!vkGetPhysicalDeviceFeatures2Fn)
    return {};

  // Build feature chain: Vulkan 1.1 → 1.2 → 1.3
  VkPhysicalDeviceVulkan13Features vk13{};
  vk13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

  VkPhysicalDeviceVulkan12Features vk12{};
  vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  vk12.pNext = &vk13;

  VkPhysicalDeviceVulkan11Features vk11{};
  vk11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
  vk11.pNext = &vk12;

  VkPhysicalDeviceFeatures2 features2{};
  features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features2.pNext = &vk11;

  // Query fills all fields with what the device supports
  vkGetPhysicalDeviceFeatures2Fn(result.physDev, &features2);

  // --- Create queue infos (1 queue per family) ---

  std::vector<VkDeviceQueueCreateInfo> queueInfos;
  float priority = 1.0f;
  for(uint32_t i = 0; i < qfCount; i++)
  {
    VkDeviceQueueCreateInfo qi{};
    qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qi.queueFamilyIndex = i;
    qi.queueCount = 1;
    qi.pQueuePriorities = &priority;
    queueInfos.push_back(qi);

    result.queueFamilies.push_back(
        {i, 1, qfProps[i].queueFlags});
  }

  // --- Create VkDevice ---

  VkDeviceCreateInfo devInfo{};
  devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  devInfo.pNext = &features2; // Features via pNext, not pEnabledFeatures
  devInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
  devInfo.pQueueCreateInfos = queueInfos.data();
  devInfo.enabledExtensionCount = static_cast<uint32_t>(extPtrs.size());
  devInfo.ppEnabledExtensionNames = extPtrs.data();

  auto vkCreateDeviceFn = reinterpret_cast<PFN_vkCreateDevice>(
      inst->getInstanceProcAddr("vkCreateDevice"));
  if(!vkCreateDeviceFn)
    return {};

  VkResult vkResult
      = vkCreateDeviceFn(result.physDev, &devInfo, nullptr, &result.dev);
  if(vkResult != VK_SUCCESS)
  {
    qDebug() << "createSharedVulkanDevice: vkCreateDevice failed:" << vkResult;
    result.dev = VK_NULL_HANDLE;
    return {};
  }

  // --- Get graphics queue ---

  auto vkGetDeviceQueueFn = reinterpret_cast<PFN_vkGetDeviceQueue>(
      inst->getInstanceProcAddr("vkGetDeviceQueue"));
  if(vkGetDeviceQueueFn)
    vkGetDeviceQueueFn(
        result.dev, result.gfxQueueFamilyIdx, 0, &result.gfxQueue);

  return result;
}

} // namespace score::gfx

#endif // VK_KHR_video_decode_queue
#endif // QT_HAS_VULKAN
