#include "DeviceInterface.hpp"


DeviceInterface::DeviceInterface(const iscore::DeviceSettings &s):
    m_settings(s)
{

}

DeviceInterface::~DeviceInterface()
{

}

const iscore::DeviceSettings &DeviceInterface::settings() const
{
    return m_settings;
}

void DeviceInterface::addNode(const iscore::Node& n)
{
    auto full = iscore::FullAddressSettings::make<iscore::FullAddressSettings::as_parent>(
                n.get<iscore::AddressSettings>(), iscore::address(*n.parent()));

    // Add in the device implementation
    addAddress(full);

    for(const auto& child : n)
    {
        addNode(child);
    }
}
