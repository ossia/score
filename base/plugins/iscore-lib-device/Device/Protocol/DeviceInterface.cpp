#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include "DeviceInterface.hpp"

namespace Device {
struct DeviceSettings;
}  // namespace iscore


DeviceInterface::DeviceInterface(const Device::DeviceSettings &s):
    m_settings(s)
{

}

DeviceInterface::~DeviceInterface()
{

}

const Device::DeviceSettings &DeviceInterface::settings() const
{
    return m_settings;
}

void DeviceInterface::addNode(const Device::Node& n)
{
    auto full = Device::FullAddressSettings::make<Device::FullAddressSettings::as_parent>(
                    n.get<Device::AddressSettings>(),
                    Device::address(*n.parent()));

    // Add in the device implementation
    addAddress(full);

    for(const auto& child : n)
    {
        addNode(child);
    }
}
