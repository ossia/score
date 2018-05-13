// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DeviceInterface.hpp"

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Device::DeviceInterface)
namespace Device
{
struct DeviceSettings;

DeviceInterface::DeviceInterface(Device::DeviceSettings s)
    : m_settings(std::move(s))
{
}

DeviceInterface::~DeviceInterface() = default;

const Device::DeviceSettings& DeviceInterface::settings() const
{
  return m_settings;
}

void DeviceInterface::addNode(const Device::Node& n)
{
  auto full = Device::FullAddressSettings::make<
      Device::FullAddressSettings::as_parent>(
      n.get<Device::AddressSettings>(), Device::address(*n.parent()));

  // Add in the device implementation
  addAddress(full);

  for (const auto& child : n)
  {
    addNode(child);
  }
}
}
