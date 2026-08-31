#pragma once
#include <score/gfx/Vulkan.hpp>

#if QT_HAS_VULKAN && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)

#include <QtGui/private/qrhivulkan_p.h>
#include <qvulkanfunctions.h>
#include <vulkan/vulkan.h>

#if __has_include(<vulkan/vulkan_win32.h>)
#include <vulkan/vulkan.h>
#ifdef Q_OS_WIN
#include <vulkan/vulkan_win32.h>
#endif
#endif

#include <QCoreApplication>
#include <QDebug>

#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#ifdef VK_KHR_video_decode_queue

namespace score::gfx
{

/// SCORE_GFX_VKDEVICE_TRACE=1 logs every VkDevice create / destroy /
/// cache hit with a CLOCK_MONOTONIC timestamp, for stall measurements.
inline bool sharedVulkanDeviceTraceEnabled()
{
  static const bool on = qEnvironmentVariableIsSet("SCORE_GFX_VKDEVICE_TRACE");
  return on;
}

inline double sharedVulkanDeviceTraceClockMs()
{
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

/// SCORE_GFX_NO_VKDEVICE_CACHE=1 sends every caller back to a private
/// per-RenderState VkDevice. An escape hatch for drivers that dislike two
/// QRhis over one device, and the control arm when measuring the cache.
inline bool sharedVulkanDeviceCacheDisabled()
{
  static const bool off
      = qEnvironmentVariableIsSet("SCORE_GFX_NO_VKDEVICE_CACHE");
  return off;
}

/// vkDestroyDevice on @p dev, resolved through @p inst.
inline void destroySharedVulkanDevice(QVulkanInstance* inst, VkDevice dev)
{
  if(!inst || dev == VK_NULL_HANDLE)
    return;
  auto fn = reinterpret_cast<PFN_vkDestroyDevice>(
      inst->getInstanceProcAddr("vkDestroyDevice"));
  if(!fn)
    return;

  const double t0 = sharedVulkanDeviceTraceClockMs();
  fn(dev, nullptr);
  if(sharedVulkanDeviceTraceEnabled())
  {
    const double t1 = sharedVulkanDeviceTraceClockMs();
    qDebug(
        "%.3f VKDEV vkDestroyDevice %.2f ms dev=%p", t1, t1 - t0, (void*)dev);
  }
}

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
  /// timelineSemaphore was queried-supported and therefore ENABLED at
  /// device creation (we enable everything the query returns). QRhi-created
  /// devices (Qt < 6.6 path) do NOT enable it — interop fast paths must
  /// check vkinterop::deviceTimelineSemaphoresEnabled().
  bool timelineSemaphores{false};
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

  void destroy() { destroy(staticVulkanInstance(false)); }

  void destroy(QVulkanInstance* inst)
  {
    destroySharedVulkanDevice(inst, dev);
    dev = VK_NULL_HANDLE;
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
 * @brief Resolve which GPU the shared VkDevice must be created on.
 *
 * Mirrors QRhi's own selection: an explicit caller request wins, else
 * QT_VK_PHYSICAL_DEVICE_INDEX, else the first enumerated device. Split out of
 * createSharedVulkanDevice so the device cache can key on the resolved GPU
 * without creating anything.
 */
inline VkPhysicalDevice resolveSharedVulkanPhysicalDevice(
    QVulkanInstance* inst, VkPhysicalDevice preferredPhysDev = VK_NULL_HANDLE)
{
  if(!inst)
    return VK_NULL_HANDLE;
  if(preferredPhysDev != VK_NULL_HANDLE)
    return preferredPhysDev;

  auto* funcs = inst->functions();
  if(!funcs)
    return VK_NULL_HANDLE;

  uint32_t devCount = 0;
  funcs->vkEnumeratePhysicalDevices(inst->vkInstance(), &devCount, nullptr);
  if(devCount == 0)
    return VK_NULL_HANDLE;

  std::vector<VkPhysicalDevice> physDevs(devCount);
  funcs->vkEnumeratePhysicalDevices(inst->vkInstance(), &devCount, physDevs.data());

  bool ok = false;
  const int idx = qEnvironmentVariableIntValue("QT_VK_PHYSICAL_DEVICE_INDEX", &ok);
  if(ok && idx >= 0 && uint32_t(idx) < devCount)
    return physDevs[uint32_t(idx)];
  return physDevs[0];
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

  // Use the caller-specified physical device (matching QRhi's GPU),
  // else honour QT_VK_PHYSICAL_DEVICE_INDEX (the same env QRhi's own
  // device selection respects — critical on multi-GPU boxes where CUDA
  // interop pins the workload to one specific GPU), else the first one.
  result.physDev = resolveSharedVulkanPhysicalDevice(inst, preferredPhysDev);
  if(result.physDev == VK_NULL_HANDLE)
    return result;

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
  result.timelineSemaphores = vk12.timelineSemaphore == VK_TRUE;

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

  const double t0 = sharedVulkanDeviceTraceClockMs();
  VkResult vkResult
      = vkCreateDeviceFn(result.physDev, &devInfo, nullptr, &result.dev);
  if(sharedVulkanDeviceTraceEnabled())
  {
    const double t1 = sharedVulkanDeviceTraceClockMs();
    qDebug("%.3f VKDEV vkCreateDevice %.2f ms physDev=%p dev=%p result=%d", t1,
           t1 - t0, (void*)result.physDev, (void*)result.dev, int(vkResult));
  }
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

/**
 * @brief Process-wide cache of imported VkDevices, keyed by (VkInstance, GPU).
 *
 * vkCreateDevice on the curated extension/feature set costs 150-210 ms and
 * vkDestroyDevice 80-140 ms; paying both on every shader-preview selection
 * froze the GUI thread for ~300 ms a click. Entries are refcounted, but a
 * refcount of zero does NOT destroy: selecting a preview tears the old
 * BackgroundNode down before building the new one, so a destroy-at-zero cache
 * would drop to zero and back to one across every transition and save nothing.
 * The cache instead owns the device for the process lifetime and releases it
 * from a qAddPostRoutine at shutdown, so a preview-to-preview transition never
 * touches the driver at all.
 *
 * Keyed on the GPU as well as the instance: createSharedVulkanDevice honours
 * QT_VK_PHYSICAL_DEVICE_INDEX and a caller-chosen preferredPhysDev, and handing
 * back a device for the wrong GPU on a multi-GPU box would be silent corruption.
 */
class SharedVulkanDeviceCache;
inline SharedVulkanDeviceCache& sharedVulkanDeviceCache();

class SharedVulkanDeviceCache
{
public:
  /// Returns a device for @p inst on the resolved GPU with its refcount
  /// incremented, or an empty device if none can be made — in which case the
  /// caller must fall back to letting QRhi create its own, exactly as it does
  /// for an uncached createSharedVulkanDevice() failure.
  SharedVulkanDevice
  acquire(QVulkanInstance* inst, VkPhysicalDevice preferredPhysDev = VK_NULL_HANDLE)
  {
    if(!inst)
      return {};

    const VkPhysicalDevice physDev
        = resolveSharedVulkanPhysicalDevice(inst, preferredPhysDev);
    if(physDev == VK_NULL_HANDLE)
      return {};

    std::lock_guard lock{m_mutex};
    for(auto& e : m_entries)
    {
      if(e.inst == inst && e.device.physDev == physDev)
      {
        e.refcount++;
        if(sharedVulkanDeviceTraceEnabled())
          qDebug(
              "%.3f VKDEV cache HIT dev=%p refcount=%d",
              sharedVulkanDeviceTraceClockMs(), (void*)e.device.dev, e.refcount);
        return e.device;
      }
    }

    SharedVulkanDevice dev = createSharedVulkanDevice(inst, physDev);
    if(!dev)
    {
      if(sharedVulkanDeviceTraceEnabled())
        qDebug(
            "%.3f VKDEV cache MISS — creation unavailable, caller falls back",
            sharedVulkanDeviceTraceClockMs());
      return {};
    }

    m_created++;
    m_entries.push_back(Entry{.inst = inst, .device = dev, .refcount = 1});
    // Per creation rather than once per process: Qt runs each post routine at
    // most once, and a process that boots a second QCoreApplication (the tests
    // do) would otherwise leave its second device undestroyed.
    qAddPostRoutine([] { sharedVulkanDeviceCache().shutdown(); });
    if(sharedVulkanDeviceTraceEnabled())
      qDebug(
          "%.3f VKDEV cache MISS dev=%p refcount=1", sharedVulkanDeviceTraceClockMs(),
          (void*)dev.dev);
    return dev;
  }

  /// Drops one reference. The device stays alive: see the class docs.
  void release(VkDevice dev)
  {
    if(dev == VK_NULL_HANDLE)
      return;
    std::lock_guard lock{m_mutex};
    for(auto& e : m_entries)
    {
      if(e.device.dev == dev)
      {
        if(e.refcount > 0)
          e.refcount--;
        if(sharedVulkanDeviceTraceEnabled())
          qDebug(
              "%.3f VKDEV cache RELEASE dev=%p refcount=%d",
              sharedVulkanDeviceTraceClockMs(), (void*)dev, e.refcount);
        return;
      }
    }
  }

  /// Destroys every idle cached device. Called from a qAddPostRoutine, i.e.
  /// after the widgets that hold references are gone but while the
  /// QVulkanInstance is still alive.
  void shutdown()
  {
    std::lock_guard lock{m_mutex};
    std::vector<Entry> stillUsed;
    for(auto& e : m_entries)
    {
      if(e.refcount != 0)
      {
        // Destroying under a live QRhi would be a use-after-free. Keep the
        // entry so a later acquire hands back the device its holders are
        // already using rather than making a second one.
        qWarning() << "SharedVulkanDeviceCache: device still referenced at "
                      "shutdown, refcount ="
                   << e.refcount;
        stillUsed.push_back(e);
        continue;
      }
      e.device.destroy(e.inst);
    }
    m_entries = std::move(stillUsed);
    m_created = int(m_entries.size());
  }

  /// How many times the cache called vkCreateDevice for the devices it
  /// currently holds — reset by shutdown(), so this counts one application
  /// session. The point of the cache is that it stays at 1 no matter how many
  /// previews that session opens.
  int createdDeviceCount() const
  {
    std::lock_guard lock{m_mutex};
    return m_created;
  }

private:
  struct Entry
  {
    QVulkanInstance* inst{};
    SharedVulkanDevice device;
    int refcount{};
  };

  mutable std::mutex m_mutex;
  std::vector<Entry> m_entries;
  int m_created{};
};

inline SharedVulkanDeviceCache& sharedVulkanDeviceCache()
{
  static SharedVulkanDeviceCache cache;
  return cache;
}

} // namespace score::gfx

#endif // VK_KHR_video_decode_queue
#endif // QT_HAS_VULKAN
