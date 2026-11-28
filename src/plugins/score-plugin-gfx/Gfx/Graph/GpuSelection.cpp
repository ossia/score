#include "GpuSelection.hpp"

#include <score/gfx/Vulkan.hpp>

#include <QDebug>
#include <QVersionNumber>

#include <atomic>
#include <cstring>

#if QT_HAS_VULKAN
#include <QVulkanFunctions>
#include <QVulkanInstance>
#endif

namespace score::gfx
{
namespace
{
std::atomic_int g_index{-1};
std::atomic<uint64_t> g_luid{};
std::atomic_bool g_hasLuid{};

#if QT_HAS_VULKAN
QString deviceType(VkPhysicalDeviceType t)
{
  switch(t)
  {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      return QStringLiteral("integrated");
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      return QStringLiteral("discrete");
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
      return QStringLiteral("virtual");
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
      return QStringLiteral("cpu");
    default:
      return {};
  }
}
#endif

std::vector<GpuDescription> enumerateGpus()
{
  std::vector<GpuDescription> res;
#if QT_HAS_VULKAN
  auto* inst = score::gfx::staticVulkanInstance();
  if(!inst || !inst->isValid())
    return res;
  auto* f = inst->functions();
  uint32_t count = 0;
  if(f->vkEnumeratePhysicalDevices(inst->vkInstance(), &count, nullptr) != VK_SUCCESS)
    return res;
  std::vector<VkPhysicalDevice> devs(count);
  f->vkEnumeratePhysicalDevices(inst->vkInstance(), &count, devs.data());

  auto getProps2 = inst->apiVersion() >= QVersionNumber(1, 1)
                       ? reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
                           inst->getInstanceProcAddr("vkGetPhysicalDeviceProperties2"))
                       : nullptr;
  for(uint32_t i = 0; i < count; i++)
  {
    VkPhysicalDeviceProperties props{};
    f->vkGetPhysicalDeviceProperties(devs[i], &props);
    GpuDescription g;
    g.index = int(i);
    g.name = QString::fromUtf8(props.deviceName);
    g.type = deviceType(props.deviceType);
    g.key = QStringLiteral("%1:%2:%3")
                .arg(props.vendorID, 4, 16, QChar('0'))
                .arg(props.deviceID, 4, 16, QChar('0'))
                .arg(g.name);

    if(getProps2 && props.apiVersion >= VK_API_VERSION_1_1)
    {
      VkPhysicalDeviceIDProperties id{};
      id.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
      VkPhysicalDeviceProperties2 props2{};
      props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
      props2.pNext = &id;
      getProps2(devs[i], &props2);
      g.key = QString::fromLatin1(
          QByteArray(reinterpret_cast<const char*>(id.deviceUUID), VK_UUID_SIZE).toHex());
      if(id.deviceLUIDValid)
      {
        GpuLuid l;
        std::memcpy(&l.low, id.deviceLUID, 4);
        std::memcpy(&l.high, id.deviceLUID + 4, 4);
        g.luid = l;
      }
    }
    res.push_back(std::move(g));
  }
#endif
  return res;
}
}

const std::vector<GpuDescription>& availableGpus()
{
  static const std::vector<GpuDescription> gpus = enumerateGpus();
  return gpus;
}

bool selectGpu(const QString& key)
{
  const GpuDescription* found{};
  if(!key.isEmpty())
  {
    for(const auto& g : availableGpus())
      if(g.key == key)
        found = &g;
    if(!found)
      qWarning() << "score::gfx: the GPU" << key
                 << "chosen in the settings is not available; using the default GPU";
  }

  g_hasLuid.store(false, std::memory_order_release);
  if(found && found->luid)
  {
    g_luid.store(
        uint64_t(found->luid->low) | (uint64_t(uint32_t(found->luid->high)) << 32),
        std::memory_order_relaxed);
    g_hasLuid.store(true, std::memory_order_release);
  }
  g_index.store(found ? found->index : -1, std::memory_order_relaxed);
  return found || key.isEmpty();
}

int selectedGpuIndex() noexcept
{
  return g_index.load(std::memory_order_relaxed);
}

std::optional<GpuLuid> selectedD3DAdapterLuid() noexcept
{
  static const bool d3dFromEnv = qEnvironmentVariableIsSet("QT_D3D_ADAPTER_INDEX");
  if(d3dFromEnv || !g_hasLuid.load(std::memory_order_acquire))
    return std::nullopt;
  const uint64_t v = g_luid.load(std::memory_order_relaxed);
  return GpuLuid{quint32(v & 0xffffffffu), qint32(uint32_t(v >> 32))};
}
}
