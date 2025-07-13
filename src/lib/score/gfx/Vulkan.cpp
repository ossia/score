#include <score/gfx/Vulkan.hpp>

#if defined(QT_FEATURE_vulkan) && QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
#include "QRhiGles2.hpp"
#include <QVulkanInstance>

#include <mutex>
namespace score::gfx
{
static QVulkanInstance* g_staticVulkanInstance{};
static std::once_flag g_staticVulkanInstanceInit{};
static bool g_staticVulkanInstanceInvalid = false;

QVulkanInstance* staticVulkanInstance(bool create)
{
  if(g_staticVulkanInstanceInvalid)
    return nullptr;

  if(!create)
    return g_staticVulkanInstance;

  std::call_once(g_staticVulkanInstanceInit, [=]() {
    g_staticVulkanInstance = new QVulkanInstance{};
    QVulkanInstance& instance = *g_staticVulkanInstance;

#if !defined(NDEBUG)
    instance.setLayers({"VK_LAYER_KHRONOS_validation"});
#endif

    QByteArrayList exts;
    exts << "VK_KHR_get_physical_device_properties2";

    if(auto v = instance.supportedApiVersion(); v >= QVersionNumber(1, 1))
    {
#if QT_VERSION < QT_VERSION_CHECK(6, 10, 0)
      // Without qtbase@3bfc5d0b3b979a8249ca1cfc38e2d3052a3c7c6f
      // we may hit vmaMemoryAllocator asserts if asking for vk 1.4
      if(v >= QVersionNumber(1, 4))
        v = QVersionNumber(1, 3);
#endif
      instance.setApiVersion(v);
    }
    else
    {
      exts << "VK_KHR_maintenance1";
    }

    instance.setExtensions(exts);
    instance.setFlags(QVulkanInstance::Flag::NoDebugOutputRedirect);

    if(!instance.create())
    {
      g_staticVulkanInstanceInvalid = true;
    }
  });

  return g_staticVulkanInstance;
}

// Returns true if the device supports ray tracing
bool supportsRayTracing(VkPhysicalDevice device, QVulkanFunctions* f)
{
  VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
  VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeature{
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
  rtFeature.pNext = &accelFeature;

  VkPhysicalDeviceFeatures2 features2{
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
  features2.pNext = &rtFeature;

  f->vkGetPhysicalDeviceFeatures2(device, &features2);

  return rtFeature.rayTracingPipeline && accelFeature.accelerationStructure;
}


// Select a Vulkan physical device, preferring one with ray tracing support
VkPhysicalDevice selectPhysicalDevice(VkInstance instance, QVulkanFunctions* f)
{
  uint32_t count = 0;
  f->vkEnumeratePhysicalDevices(instance, &count, nullptr);
  if (count == 0)
    qFatal("No Vulkan physical devices found");

  std::vector<VkPhysicalDevice> devices(count);
  f->vkEnumeratePhysicalDevices(instance, &count, devices.data());

  VkPhysicalDevice fallbackDiscrete = VK_NULL_HANDLE;

  for (VkPhysicalDevice dev : devices)
  {
    VkPhysicalDeviceProperties props;
    f->vkGetPhysicalDeviceProperties(dev, &props);

    QString name = QString::fromUtf8(props.deviceName);
    qDebug() << "Found GPU:" << name
             << "| Vendor:" << QString::number(props.vendorID, 16)
             << "| Type:" << props.deviceType;

    if (supportsRayTracing(dev, f))
    {
      qDebug() << "→ Selected (has ray tracing)";
      return dev;
    }

    if (!fallbackDiscrete && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
      fallbackDiscrete = dev;
  }

  if (fallbackDiscrete)
  {
    qDebug() << "→ Selected fallback discrete GPU";
    return fallbackDiscrete;
  }

  qDebug() << "→ Selected default device[0]";
  return devices[0];
}


// Create a Vulkan device with ray tracing and descriptor indexing support
VkDevice createRayTracingDevice(
    VkInstance instance, VkPhysicalDevice physDev,
    uint32_t& queueFamilyIndex, QVulkanFunctions* f)
{
  // 1. Required extensions
  const std::vector<const char*> deviceExtensions = {
    "VK_KHR_swapchain",
    "VK_KHR_ray_tracing_pipeline",
    "VK_KHR_acceleration_structure",
    "VK_KHR_deferred_host_operations",
    "VK_KHR_buffer_device_address",
    "VK_KHR_spirv_1_4",
    "VK_KHR_shader_float_controls",
    "VK_EXT_descriptor_indexing",
    "VK_KHR_maintenance3",
    "VK_KHR_get_memory_requirements2",
    "VK_KHR_dedicated_allocation"
  };

  // 2. Find a graphics queue family
  uint32_t count = 0;
  f->vkGetPhysicalDeviceQueueFamilyProperties(physDev, &count, nullptr);
  std::vector<VkQueueFamilyProperties> families(count);
  f->vkGetPhysicalDeviceQueueFamilyProperties(physDev, &count, families.data());

  queueFamilyIndex = UINT32_MAX;
  for (uint32_t i = 0; i < count; ++i) {
    if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      queueFamilyIndex = i;
      break;
    }
  }

  if (queueFamilyIndex == UINT32_MAX)
    qFatal("Failed to find graphics queue family");

  float priority = 1.0f;
  VkDeviceQueueCreateInfo queueInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
  queueInfo.queueFamilyIndex = queueFamilyIndex;
  queueInfo.queueCount = 1;
  queueInfo.pQueuePriorities = &priority;

  // 3. Feature chain
  VkPhysicalDeviceBufferDeviceAddressFeatures bufferFeature{
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
  bufferFeature.bufferDeviceAddress = VK_TRUE;

  VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeature{
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
  rtFeature.rayTracingPipeline = VK_TRUE;
  rtFeature.pNext = &bufferFeature;

  VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
  accelFeature.accelerationStructure = VK_TRUE;
  accelFeature.pNext = &rtFeature;

  VkPhysicalDeviceDescriptorIndexingFeatures indexingFeature{
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES };
  indexingFeature.pNext = &accelFeature;

  VkPhysicalDeviceFeatures2 features2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
  features2.pNext = &indexingFeature;
  f->vkGetPhysicalDeviceFeatures2(physDev, &features2);

  // 4. Create the device
  VkDeviceCreateInfo createInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
  createInfo.pNext = &features2;
  createInfo.queueCreateInfoCount = 1;
  createInfo.pQueueCreateInfos = &queueInfo;
  createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
  createInfo.ppEnabledExtensionNames = deviceExtensions.data();
  createInfo.pEnabledFeatures = &features2.features;

  VkDevice device = VK_NULL_HANDLE;
  if (f->vkCreateDevice(physDev, &createInfo, nullptr, &device) != VK_SUCCESS)
    qFatal("Failed to create Vulkan device with ray tracing support");

  return device;
}


// Create QRhi-compatible native Vulkan handles with ray tracing support
QRhiVulkanNativeHandles* createRayTracingRhi(QVulkanInstance* inst)
{
  if (!inst->isValid())
  {
    inst->setApiVersion(QVersionNumber(1, 2, 0));

    auto exts = inst->extensions();
    if (!exts.contains("VK_KHR_get_physical_device_properties2"))
      exts << "VK_KHR_get_physical_device_properties2";

    inst->setExtensions(exts);
    inst->setFlags(QVulkanInstance::NoDebugOutputRedirect);

    if (!inst->create())
      qFatal("Failed to create Vulkan instance");
  }

  QVulkanFunctions* f = inst->functions();
  VkPhysicalDevice physDev = selectPhysicalDevice(inst->vkInstance(), f);

  uint32_t queueFamily = 0;
  VkDevice device = createRayTracingDevice(inst->vkInstance(), physDev, queueFamily, f);

  auto* handles = new QRhiVulkanNativeHandles;
  handles->inst = inst;
  handles->physDev = physDev;
  handles->dev = device;
  handles->gfxQueueFamilyIdx = queueFamily;
  handles->gfxQueueIdx = 0;

  return handles;
}



}
#endif
