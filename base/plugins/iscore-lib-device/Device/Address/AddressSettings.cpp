#include <QStringList>

#include "AddressSettings.hpp"
#include <ossia/editor/state/destination_qualifiers.hpp>
#include <ossia/network/domain/domain.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <State/Address.hpp>
#include <State/Message.hpp>
#include <State/Value.hpp>
namespace Device
{

AddressSettingsCommon::AddressSettingsCommon() noexcept
{
}

AddressSettingsCommon::AddressSettingsCommon(
    const AddressSettingsCommon& other) noexcept
    : value{other.value}
    , domain{other.domain}
    , ioType{other.ioType}
    , clipMode{other.clipMode}
    , unit{other.unit}
    , repetitionFilter{other.repetitionFilter}
    , rate{other.rate}
    , priority{other.priority}
    , tags{other.tags}
    , description{other.description}
{
}

AddressSettingsCommon::AddressSettingsCommon(
    AddressSettingsCommon&& other) noexcept
    : value{std::move(other.value)}
    , domain{std::move(other.domain)}
    , ioType{other.ioType}
    , clipMode{other.clipMode}
    , unit{std::move(other.unit)}
    , repetitionFilter{other.repetitionFilter}
    , rate{other.rate}
    , priority{other.priority}
    , tags{std::move(other.tags)}
    , description{std::move(other.description)}
{
}

AddressSettingsCommon& AddressSettingsCommon::
operator=(const AddressSettingsCommon& other) noexcept
{
  value = other.value;
  domain = other.domain;
  ioType = other.ioType;
  clipMode = other.clipMode;
  unit = other.unit;
  repetitionFilter = other.repetitionFilter;
  rate = other.rate;
  priority = other.priority;
  tags = other.tags;
  description = other.description;
  return *this;
}

AddressSettingsCommon& AddressSettingsCommon::
operator=(AddressSettingsCommon&& other) noexcept
{
  value = std::move(other.value);
  domain = std::move(other.domain);
  ioType = other.ioType;
  clipMode = other.clipMode;
  unit = std::move(other.unit);
  repetitionFilter = other.repetitionFilter;
  rate = other.rate;
  priority = other.priority;
  tags = std::move(other.tags);
  description = std::move(other.description);
  return *this;
}

AddressSettingsCommon::~AddressSettingsCommon() noexcept
{
}

AddressSettings::AddressSettings() noexcept = default;
AddressSettings::AddressSettings(const AddressSettings&) noexcept = default;
AddressSettings::AddressSettings(AddressSettings&&) noexcept = default;
AddressSettings& AddressSettings::operator=(const AddressSettings&) noexcept
    = default;
AddressSettings& AddressSettings::operator=(AddressSettings&&) noexcept
    = default;
AddressSettings::~AddressSettings() noexcept = default;

FullAddressSettings::FullAddressSettings() noexcept = default;
FullAddressSettings::FullAddressSettings(const FullAddressSettings&) noexcept
    = default;
FullAddressSettings::FullAddressSettings(FullAddressSettings&&) noexcept
    = default;
FullAddressSettings& FullAddressSettings::
operator=(const FullAddressSettings&) noexcept
    = default;
FullAddressSettings& FullAddressSettings::
operator=(FullAddressSettings&&) noexcept
    = default;
FullAddressSettings::~FullAddressSettings() noexcept = default;

FullAddressAccessorSettings::FullAddressAccessorSettings() noexcept
{
}

FullAddressAccessorSettings::FullAddressAccessorSettings(
    const FullAddressAccessorSettings& other) noexcept
    : value{other.value}
    , domain{other.domain}
    , ioType{other.ioType}
    , clipMode{other.clipMode}
    , repetitionFilter{other.repetitionFilter}
    , rate{other.rate}
    , priority{other.priority}
    , tags{other.tags}
    , description{other.description}
    , address{other.address}
{
}

FullAddressAccessorSettings::FullAddressAccessorSettings(
    FullAddressAccessorSettings&& other) noexcept
    : value{std::move(other.value)}
    , domain{std::move(other.domain)}
    , ioType{other.ioType}
    , clipMode{other.clipMode}
    , repetitionFilter{other.repetitionFilter}
    , rate{other.rate}
    , priority{other.priority}
    , tags{std::move(other.tags)}
    , description{std::move(other.description)}
    , address{std::move(other.address)}
{
}

FullAddressAccessorSettings& FullAddressAccessorSettings::
operator=(const FullAddressAccessorSettings& other) noexcept
{
  value = other.value;
  domain = other.domain;
  ioType = other.ioType;
  clipMode = other.clipMode;
  repetitionFilter = other.repetitionFilter;
  rate = other.rate;
  priority = other.priority;
  tags = other.tags;
  description = other.description;
  address = other.address;
  return *this;
}

FullAddressAccessorSettings& FullAddressAccessorSettings::
operator=(FullAddressAccessorSettings&& other) noexcept
{
  value = std::move(other.value);
  domain = std::move(other.domain);
  ioType = other.ioType;
  clipMode = other.clipMode;
  repetitionFilter = other.repetitionFilter;
  rate = other.rate;
  priority = other.priority;
  tags = std::move(other.tags);
  description = std::move(other.description);
  address = std::move(other.address);
  return *this;
}

FullAddressAccessorSettings::~FullAddressAccessorSettings() noexcept
{
}
// Second argument should be the address of the parent.
template <>
ISCORE_LIB_DEVICE_EXPORT FullAddressSettings
FullAddressSettings::make<FullAddressSettings::as_parent>(
    const Device::AddressSettings& other, const State::Address& addr) noexcept
{
  FullAddressSettings as;
  static_cast<AddressSettingsCommon&>(as) = other;

  as.address = addr;
  as.address.path.append(other.name);

  return as;
}

// Second argument should be the address of the resulting FullAddressSettings.
template <>
ISCORE_LIB_DEVICE_EXPORT FullAddressSettings
FullAddressSettings::make<FullAddressSettings::as_child>(
    const Device::AddressSettings& other, const State::Address& addr) noexcept
{
  FullAddressSettings as;
  static_cast<AddressSettingsCommon&>(as) = other;

  as.address = addr;

  return as;
}

ISCORE_LIB_DEVICE_EXPORT FullAddressSettings
FullAddressSettings::make(const State::Message& mess) noexcept
{
  FullAddressSettings as;

  as.address = mess.address.address;
  as.value = mess.value;

  return as;
}

ISCORE_LIB_DEVICE_EXPORT FullAddressSettings
FullAddressSettings::make(const Node& node) noexcept
{
  ISCORE_ASSERT(node.is<Device::AddressSettings>());
  auto& other = node.get<Device::AddressSettings>();

  FullAddressSettings as;
  static_cast<AddressSettingsCommon&>(as) = other;
  as.address = Device::address(node).address;

  return as;
}

FullAddressAccessorSettings::FullAddressAccessorSettings(
    const State::AddressAccessor& addr, const AddressSettingsCommon& f) noexcept
    : value{f.value}
    , domain{f.domain}
    , ioType{f.ioType}
    , clipMode{f.clipMode}
    , repetitionFilter{f.repetitionFilter}
    , rate{f.rate}
    , priority{f.priority}
    , tags{f.tags}
    , description{f.description}
    , address{addr}
{
  if (!address.qualifiers.get().unit)
    address.qualifiers.get().unit = f.unit;
}


FullAddressAccessorSettings::FullAddressAccessorSettings(
    const State::AddressAccessor& addr,
    const ossia::value& min,
    const ossia::value& max) noexcept
    : domain{ossia::net::make_domain(min, max)}, address{addr}
{
}

FullAddressAccessorSettings::FullAddressAccessorSettings(
    State::AddressAccessor&& addr, AddressSettingsCommon&& f) noexcept
    : value{std::move(f.value)}
    , domain{std::move(f.domain)}
    , ioType{f.ioType}
    , clipMode{f.clipMode}
    , repetitionFilter{f.repetitionFilter}
    , rate{f.rate}
    , priority{f.priority}
    , tags{std::move(f.tags)}
    , description{std::move(f.description)}
    , address{std::move(addr)}
{
  if (!address.qualifiers.get().unit)
    address.qualifiers.get().unit = f.unit;
}

FullAddressAccessorSettings::FullAddressAccessorSettings(
    FullAddressSettings&& f) noexcept
  : FullAddressAccessorSettings{
      State::AddressAccessor{std::move(f.address)},
      std::move(static_cast<AddressSettingsCommon&&>(f))}
{

}

bool operator==(
    const AddressSettingsCommon& lhs, const AddressSettingsCommon& rhs) noexcept
{
  return lhs.value == rhs.value && lhs.domain == rhs.domain
         && lhs.ioType == rhs.ioType && lhs.clipMode == rhs.clipMode
         && lhs.unit == rhs.unit
         && lhs.repetitionFilter == rhs.repetitionFilter
         && lhs.rate == rhs.rate && lhs.priority == rhs.priority
         && lhs.tags == rhs.tags;
}
}
