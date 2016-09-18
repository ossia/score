#include <QStringList>

#include "AddressSettings.hpp"
#include <State/Address.hpp>
#include <State/Message.hpp>
#include <State/Value.hpp>
#include <Device/Node/DeviceNode.hpp>

namespace Device
{

// Second argument should be the address of the parent.
template<>
ISCORE_LIB_DEVICE_EXPORT FullAddressSettings FullAddressSettings::make<FullAddressSettings::as_parent>(
        const Device::AddressSettings& other,
        const State::Address& addr)
{
    FullAddressSettings as;
    static_cast<AddressSettingsCommon&>(as) = other;

    as.address = addr;
    as.address.path.append(other.name);

    return as;
}

// Second argument should be the address of the resulting FullAddressSettings.
template<>
ISCORE_LIB_DEVICE_EXPORT FullAddressSettings FullAddressSettings::make<FullAddressSettings::as_child>(
        const Device::AddressSettings& other,
        const State::Address& addr)
{
    FullAddressSettings as;
    static_cast<AddressSettingsCommon&>(as) = other;

    as.address = addr;

    return as;
}

ISCORE_LIB_DEVICE_EXPORT FullAddressSettings FullAddressSettings::make(
        const State::Message& mess)
{
    FullAddressSettings as;

    as.address = mess.address.address;
    as.value = mess.value;

    return as;
}

ISCORE_LIB_DEVICE_EXPORT FullAddressSettings FullAddressSettings::make(
        const Node& node)
{
    ISCORE_ASSERT(node.is<Device::AddressSettings>());
    auto& other = node.get<Device::AddressSettings>();

    FullAddressSettings as;
    static_cast<AddressSettingsCommon&>(as) = other;
    as.address = Device::address(node).address;

    return as;
}
}
