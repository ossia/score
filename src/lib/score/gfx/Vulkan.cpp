#include <score/gfx/Vulkan.hpp>

#if defined(QT_FEATURE_vulkan) && QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
#include <QVulkanInstance>

namespace score::gfx
{
QVulkanInstance* staticVulkanInstance()
{
  static bool created = false;
  static bool invalid = false;
  if(invalid)
    return nullptr;

  static QVulkanInstance instance;
  if(created)
    return &instance;

#if !defined(NDEBUG)
  instance.setLayers({"VK_LAYER_KHRONOS_validation"});
#endif

  QByteArrayList exts;
  exts << "VK_KHR_get_physical_device_properties2";

  if(auto v = instance.supportedApiVersion(); v >= QVersionNumber(1, 1, 0))
    instance.setApiVersion(v);
  else
    exts << "VK_KHR_maintenance1";

  instance.setExtensions(exts);
  instance.setFlags(QVulkanInstance::Flag::NoDebugOutputRedirect);

  if(!instance.create())
  {
    invalid = true;
    return nullptr;
  }
  created = true;
  return &instance;
}
}
#endif
