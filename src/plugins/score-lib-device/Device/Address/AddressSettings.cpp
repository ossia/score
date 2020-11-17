// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddressSettings.hpp"

#include <Device/Node/DeviceNode.hpp>
#include <State/Address.hpp>
#include <State/Message.hpp>
#include <State/Value.hpp>

#include <ossia/network/common/destination_qualifiers.hpp>
#include <ossia/network/domain/domain.hpp>

namespace Device
{

AddressSettingsCommon::AddressSettingsCommon() noexcept { }

AddressSettingsCommon::AddressSettingsCommon(const AddressSettingsCommon& other) noexcept
    : value{other.value}
    , domain{other.domain}
    , unit{other.unit}
    , ioType{other.ioType}
    , clipMode{other.clipMode}
    , repetitionFilter{other.repetitionFilter}
    , extendedAttributes{other.extendedAttributes}
{
}

AddressSettingsCommon::AddressSettingsCommon(AddressSettingsCommon&& other) noexcept
    : value{std::move(other.value)}
    , domain{std::move(other.domain)}
    , unit{std::move(other.unit)}
    , ioType{other.ioType}
    , clipMode{other.clipMode}
    , repetitionFilter{other.repetitionFilter}
    , extendedAttributes{std::move(other.extendedAttributes)}
{
}

AddressSettingsCommon&
AddressSettingsCommon::operator=(const AddressSettingsCommon& other) noexcept
{
  value = other.value;
  domain = other.domain;
  unit = other.unit;
  ioType = other.ioType;
  clipMode = other.clipMode;
  repetitionFilter = other.repetitionFilter;
  extendedAttributes = other.extendedAttributes;
  return *this;
}

AddressSettingsCommon& AddressSettingsCommon::operator=(AddressSettingsCommon&& other) noexcept
{
  value = std::move(other.value);
  domain = std::move(other.domain);
  unit = std::move(other.unit);
  ioType = other.ioType;
  clipMode = other.clipMode;
  repetitionFilter = other.repetitionFilter;
  extendedAttributes = std::move(other.extendedAttributes);
  return *this;
}

AddressSettingsCommon::~AddressSettingsCommon() noexcept { }

AddressSettings::AddressSettings() noexcept = default;
AddressSettings::AddressSettings(const AddressSettings&) noexcept = default;
AddressSettings::AddressSettings(AddressSettings&&) noexcept = default;
AddressSettings& AddressSettings::operator=(const AddressSettings&) noexcept = default;
AddressSettings& AddressSettings::operator=(AddressSettings&&) noexcept = default;
AddressSettings::~AddressSettings() noexcept = default;

FullAddressSettings::FullAddressSettings() noexcept = default;
FullAddressSettings::FullAddressSettings(const FullAddressSettings&) noexcept = default;
FullAddressSettings::FullAddressSettings(FullAddressSettings&&) noexcept = default;
FullAddressSettings& FullAddressSettings::operator=(const FullAddressSettings&) noexcept = default;
FullAddressSettings& FullAddressSettings::operator=(FullAddressSettings&&) noexcept = default;
FullAddressSettings::~FullAddressSettings() noexcept = default;

FullAddressAccessorSettings::FullAddressAccessorSettings() noexcept { }

FullAddressAccessorSettings::FullAddressAccessorSettings(
    const FullAddressAccessorSettings& other) noexcept
    : value{other.value}
    , domain{other.domain}
    , ioType{other.ioType}
    , clipMode{other.clipMode}
    , repetitionFilter{other.repetitionFilter}
    , extendedAttributes{other.extendedAttributes}
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
    , extendedAttributes{std::move(other.extendedAttributes)}
    , address{std::move(other.address)}
{
}

FullAddressAccessorSettings&
FullAddressAccessorSettings::operator=(const FullAddressAccessorSettings& other) noexcept
{
  value = other.value;
  domain = other.domain;
  ioType = other.ioType;
  clipMode = other.clipMode;
  repetitionFilter = other.repetitionFilter;
  extendedAttributes = other.extendedAttributes;
  address = other.address;
  return *this;
}

FullAddressAccessorSettings&
FullAddressAccessorSettings::operator=(FullAddressAccessorSettings&& other) noexcept
{
  value = std::move(other.value);
  domain = std::move(other.domain);
  ioType = other.ioType;
  clipMode = other.clipMode;
  repetitionFilter = other.repetitionFilter;
  extendedAttributes = std::move(other.extendedAttributes);
  address = std::move(other.address);
  return *this;
}

FullAddressAccessorSettings::~FullAddressAccessorSettings() noexcept { }
// Second argument should be the address of the parent.
template <>
SCORE_LIB_DEVICE_EXPORT FullAddressSettings
FullAddressSettings::make<FullAddressSettings::as_parent>(
    const Device::AddressSettings& other,
    const State::Address& addr) noexcept
{
  FullAddressSettings as;
  static_cast<AddressSettingsCommon&>(as) = other;

  as.address = addr;
  as.address.path.append(other.name);

  return as;
}

// Second argument should be the address of the resulting FullAddressSettings.
template <>
SCORE_LIB_DEVICE_EXPORT FullAddressSettings
FullAddressSettings::make<FullAddressSettings::as_child>(
    const Device::AddressSettings& other,
    const State::Address& addr) noexcept
{
  FullAddressSettings as;
  static_cast<AddressSettingsCommon&>(as) = other;

  as.address = addr;

  return as;
}

SCORE_LIB_DEVICE_EXPORT FullAddressSettings
FullAddressSettings::make(const State::Message& mess) noexcept
{
  FullAddressSettings as;

  as.address = mess.address.address;
  as.unit = mess.address.qualifiers.get().unit;
  as.value = mess.value;

  return as;
}

FullAddressAccessorSettings::FullAddressAccessorSettings(
    const State::AddressAccessor& addr,
    const AddressSettingsCommon& f) noexcept
    : value{f.value}
    , domain{f.domain}
    , ioType{f.ioType}
    , clipMode{f.clipMode}
    , repetitionFilter{f.repetitionFilter}
    , extendedAttributes{f.extendedAttributes}
    , address{addr}
{
  if (!address.qualifiers.get().unit)
    address.qualifiers.get().unit = f.unit;
}

FullAddressAccessorSettings::FullAddressAccessorSettings(
    const State::AddressAccessor& addr,
    const ossia::value& min,
    const ossia::value& max) noexcept
    : domain{ossia::make_domain(min, max)}, address{addr}
{
}

FullAddressAccessorSettings::FullAddressAccessorSettings(
    State::AddressAccessor&& addr,
    AddressSettingsCommon&& f) noexcept
    : value{std::move(f.value)}
    , domain{std::move(f.domain)}
    , ioType{f.ioType}
    , clipMode{f.clipMode}
    , repetitionFilter{f.repetitionFilter}
    , extendedAttributes{std::move(f.extendedAttributes)}
    , address{std::move(addr)}
{
  if (!address.qualifiers.get().unit)
    address.qualifiers.get().unit = f.unit;
}

FullAddressAccessorSettings::FullAddressAccessorSettings(FullAddressSettings&& f) noexcept
    : FullAddressAccessorSettings{
        State::AddressAccessor{std::move(f.address)},
        std::move(static_cast<AddressSettingsCommon&&>(f))}
{
}

bool operator==(const AddressSettingsCommon& lhs, const AddressSettingsCommon& rhs) noexcept
{
  return lhs.value == rhs.value && lhs.domain == rhs.domain && lhs.ioType == rhs.ioType
         && lhs.clipMode == rhs.clipMode && lhs.unit == rhs.unit
         && lhs.repetitionFilter == rhs.repetitionFilter
      // TODO we cannot compare "any" values...   && lhs.extendedAttributes ==
      // rhs.extendedAttributes
      ;
}
}
