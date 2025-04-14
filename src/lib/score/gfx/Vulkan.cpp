#include <score/gfx/Vulkan.hpp>

#if defined(QT_FEATURE_vulkan) && QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
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
}
#endif
