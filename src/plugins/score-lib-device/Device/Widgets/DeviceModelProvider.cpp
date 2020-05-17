#include <Device/Widgets/DeviceModelProvider.hpp>

namespace Device
{
DeviceModelProvider::~DeviceModelProvider() { }

DeviceModelProviderList::~DeviceModelProviderList() { }

DeviceModelProvider*
DeviceModelProviderList::getBestProvider(const score::DocumentContext& ctx) const noexcept
{
  if (!empty())
    return &*begin();
  return nullptr;
}
}
