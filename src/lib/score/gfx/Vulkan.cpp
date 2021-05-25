#include <score/gfx/Vulkan.hpp>

#if QT_CONFIG(vulkan)
#include <QVulkanInstance>
#endif

namespace score::gfx
{

#if QT_CONFIG(vulkan)
QVulkanInstance* staticVulkanInstance()
{
  static bool created = false;
  static bool invalid = false;
  if (invalid)
    return nullptr;

  static QVulkanInstance instance;
  if (created)
    return &instance;

#if !defined(NDEBUG)
  instance.setLayers({"VK_LAYER_KHRONOS_validation"});
#endif
  instance.setExtensions(
      QByteArrayList() << "VK_KHR_get_physical_device_properties2");

  if (!instance.create())
  {
    invalid = true;
    return nullptr;
  }
  created = true;
  return &instance;
}
#endif

}
