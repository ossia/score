// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "NodeBasedItemModel.hpp"

#include <ossia/network/domain/domain.hpp>
#include <ossia/network/domain/domain_functions.hpp>
namespace Device
{
NodeBasedItemModel::~NodeBasedItemModel() = default;

Device::FullAddressAccessorSettings makeFullAddressAccessorSettings(
    const State::AddressAccessor& addr,
    const Device::NodeBasedItemModel& deviceexplorer,
    ossia::value min,
    ossia::value max)
{
  auto& newval = addr.address;

  auto newpath = newval.path;
  newpath.prepend(newval.device);

  // First try to find if there is a matching address
  // in the device explorer
  auto new_n = Device::try_getNodeFromString(deviceexplorer.rootNode(), newpath);
  if (new_n && new_n->is<Device::AddressSettings>())
  {
    return Device::FullAddressAccessorSettings{addr, new_n->get<Device::AddressSettings>()};
  }
  else
  {
    // TODO Try also with the OSSIA conversions.
    // But this requires refactoring quite a bit...
  }

  // If there is none, build with some default settings
  Device::FullAddressAccessorSettings s;
  s.address = addr;
  s.domain = ossia::make_domain(std::move(min), std::move(max));
  return s;
}

Device::FullAddressAccessorSettings makeFullAddressAccessorSettings(const Device::Node& mess)
{
  if (auto as_ptr = mess.target<Device::AddressSettings>())
  {
    return Device::FullAddressAccessorSettings{Device::address(mess), *as_ptr};
  }
  return {};
}
}
