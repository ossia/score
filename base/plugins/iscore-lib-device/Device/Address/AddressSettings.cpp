#include <QStringList>

#include "AddressSettings.hpp"
#include <State/Address.hpp>
#include <State/Message.hpp>
#include <State/Value.hpp>
#include <Device/Node/DeviceNode.hpp>

namespace Device
{

AddressSettingsCommon::AddressSettingsCommon() = default;
AddressSettingsCommon::AddressSettingsCommon(const AddressSettingsCommon& ) = default;
AddressSettingsCommon::AddressSettingsCommon(AddressSettingsCommon&& ) = default;
AddressSettingsCommon& AddressSettingsCommon::operator=(const AddressSettingsCommon& ) = default;
AddressSettingsCommon& AddressSettingsCommon::operator=(AddressSettingsCommon&& ) = default;
AddressSettingsCommon::~AddressSettingsCommon() = default;

AddressSettings::AddressSettings() = default;
AddressSettings::AddressSettings(const AddressSettings& ) = default;
AddressSettings::AddressSettings(AddressSettings&& ) = default;
AddressSettings& AddressSettings::operator=(const AddressSettings& ) = default;
AddressSettings& AddressSettings::operator=(AddressSettings&& ) = default;
AddressSettings::~AddressSettings() = default;

FullAddressSettings::FullAddressSettings() = default;
FullAddressSettings::FullAddressSettings(const FullAddressSettings& ) = default;
FullAddressSettings::FullAddressSettings(FullAddressSettings&& ) = default;
FullAddressSettings& FullAddressSettings::operator=(const FullAddressSettings& ) = default;
FullAddressSettings& FullAddressSettings::operator=(FullAddressSettings&& ) = default;
FullAddressSettings::~FullAddressSettings() = default;

FullAddressAccessorSettings::FullAddressAccessorSettings() = default;
FullAddressAccessorSettings::FullAddressAccessorSettings(const FullAddressAccessorSettings& ) = default;
FullAddressAccessorSettings::FullAddressAccessorSettings(FullAddressAccessorSettings&& ) = default;
FullAddressAccessorSettings& FullAddressAccessorSettings::operator=(const FullAddressAccessorSettings& ) = default;
FullAddressAccessorSettings& FullAddressAccessorSettings::operator=(FullAddressAccessorSettings&& ) = default;
FullAddressAccessorSettings::~FullAddressAccessorSettings() = default;

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

FullAddressAccessorSettings::FullAddressAccessorSettings(const State::AddressAccessor &addr, const AddressSettingsCommon &f):
    value{f.value},
    domain{f.domain},
    ioType{f.ioType},
    clipMode{f.clipMode},
    repetitionFilter{f.repetitionFilter},
    rate{f.rate},
    priority{f.priority},
    tags{f.tags},
    description{f.description},
    address{addr}
{
    if(!address.qualifiers.unit)
        address.qualifiers.unit = f.unit;
}

FullAddressAccessorSettings::FullAddressAccessorSettings(const State::AddressAccessor &addr, const ossia::value &min, const ossia::value &max):
    domain{ossia::net::make_domain(min, max)},
    address{addr}
{

}

FullAddressAccessorSettings::FullAddressAccessorSettings(State::AddressAccessor &&addr, AddressSettingsCommon &&f):
    value{std::move(f.value)},
    domain{std::move(f.domain)},
    ioType{f.ioType},
    clipMode{f.clipMode},
    repetitionFilter{f.repetitionFilter},
    rate{f.rate},
    priority{f.priority},
    tags{std::move(f.tags)},
    description{std::move(f.description)},
    address{std::move(addr)}
{
    if(!address.qualifiers.unit)
        address.qualifiers.unit = f.unit;
}

bool operator==(const AddressSettingsCommon &lhs, const AddressSettingsCommon &rhs)
{
    return
            lhs.value == rhs.value
            && lhs.domain == rhs.domain
            && lhs.ioType == rhs.ioType
            && lhs.clipMode == rhs.clipMode
            && lhs.unit == rhs.unit
            && lhs.repetitionFilter == rhs.repetitionFilter
            && lhs.rate == rhs.rate
            && lhs.priority == rhs.priority
            && lhs.tags == rhs.tags;
}

}
