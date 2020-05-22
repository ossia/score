#pragma once
#include <Device/Address/ClipMode.hpp>
#include <Device/Address/IOType.hpp>
#include <State/Domain.hpp>
#include <State/Message.hpp>
#include <State/Unit.hpp>

#include <score/tools/Metadata.hpp>

#include <ossia/detail/any_map.hpp>
//#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/common/parameter_properties.hpp>

#include <QString>

#include <score_lib_device_export.h>

namespace Device
{

using RepetitionFilter = bool;
struct SCORE_LIB_DEVICE_EXPORT AddressSettingsCommon
{
  AddressSettingsCommon() noexcept;
  AddressSettingsCommon(const AddressSettingsCommon&) noexcept;
  AddressSettingsCommon(AddressSettingsCommon&&) noexcept;
  AddressSettingsCommon& operator=(const AddressSettingsCommon&) noexcept;
  AddressSettingsCommon& operator=(AddressSettingsCommon&&) noexcept;
  ~AddressSettingsCommon() noexcept;

  ossia::value value;
  State::Domain domain;

  State::Unit unit;

  std::optional<ossia::access_mode> ioType;
  ossia::bounding_mode clipMode{};
  ossia::repetition_filter repetitionFilter{};

  ossia::extended_attributes extendedAttributes;

  operator const ossia::extended_attributes &() const { return extendedAttributes; }
  operator ossia::extended_attributes &() { return extendedAttributes; }
};

// this one has only the name of the current node (e.g. 'a' for dev:/azazd/a)
struct SCORE_LIB_DEVICE_EXPORT AddressSettings : public Device::AddressSettingsCommon
{
  AddressSettings() noexcept;
  AddressSettings(const AddressSettings&) noexcept;
  AddressSettings(AddressSettings&&) noexcept;
  AddressSettings& operator=(const AddressSettings&) noexcept;
  AddressSettings& operator=(AddressSettings&&) noexcept;
  ~AddressSettings() noexcept;

  QString name;
};

// This one has the whole path of the node in address
struct SCORE_LIB_DEVICE_EXPORT FullAddressSettings : public Device::AddressSettingsCommon
{
  FullAddressSettings() noexcept;
  FullAddressSettings(const FullAddressSettings&) noexcept;
  FullAddressSettings(FullAddressSettings&&) noexcept;
  FullAddressSettings& operator=(const FullAddressSettings&) noexcept;
  FullAddressSettings& operator=(FullAddressSettings&&) noexcept;
  ~FullAddressSettings() noexcept;

  // Maybe we should just use FullAddressSettings behind a flyweight
  // pattern everywhere... (see mnmlstc/flyweight)
  struct as_parent;
  struct as_child;
  State::Address address;

  template <typename T>
  SCORE_LIB_DEVICE_EXPORT static FullAddressSettings
  make(const Device::AddressSettings& other, const State::Address& addr) noexcept;
  template <typename T>
  static FullAddressSettings
  make(const Device::AddressSettings& other, const State::AddressAccessor& addr) noexcept
  {
    return make<T>(other, addr.address);
  }

  static FullAddressSettings make(const State::Message& mess) noexcept;

  // Specializations are in FullAddressSettings.cpp
};

SCORE_LIB_DEVICE_EXPORT bool operator==(
    const Device::AddressSettingsCommon& lhs,
    const Device::AddressSettingsCommon& rhs) noexcept;

inline bool operator!=(
    const Device::AddressSettingsCommon& lhs,
    const Device::AddressSettingsCommon& rhs) noexcept
{
  return !(lhs == rhs);
}
inline bool
operator==(const Device::AddressSettings& lhs, const Device::AddressSettings& rhs) noexcept
{
  return static_cast<const Device::AddressSettingsCommon&>(lhs)
             == static_cast<const Device::AddressSettingsCommon&>(rhs)
         && lhs.name == rhs.name;
}

inline bool
operator!=(const Device::AddressSettings& lhs, const Device::AddressSettings& rhs) noexcept
{
  return !(lhs == rhs);
}
inline bool
operator==(const Device::FullAddressSettings& lhs, const Device::FullAddressSettings& rhs) noexcept
{
  return static_cast<const Device::AddressSettingsCommon&>(lhs)
             == static_cast<const Device::AddressSettingsCommon&>(rhs)
         && lhs.address == rhs.address;
}

inline bool
operator!=(const Device::FullAddressSettings& lhs, const Device::FullAddressSettings& rhs) noexcept
{
  return !(lhs == rhs);
}

struct SCORE_LIB_DEVICE_EXPORT FullAddressAccessorSettings
{
  FullAddressAccessorSettings() noexcept;
  FullAddressAccessorSettings(const FullAddressAccessorSettings&) noexcept;
  FullAddressAccessorSettings(FullAddressAccessorSettings&&) noexcept;
  FullAddressAccessorSettings& operator=(const FullAddressAccessorSettings&) noexcept;
  FullAddressAccessorSettings& operator=(FullAddressAccessorSettings&&) noexcept;
  ~FullAddressAccessorSettings() noexcept;

  FullAddressAccessorSettings(
      const State::AddressAccessor& addr,
      const AddressSettingsCommon& f) noexcept;

  explicit FullAddressAccessorSettings(FullAddressSettings&& f) noexcept;

  FullAddressAccessorSettings(State::AddressAccessor&& addr, AddressSettingsCommon&& f) noexcept;

  FullAddressAccessorSettings(
      const State::AddressAccessor& addr,
      const ossia::value& min,
      const ossia::value& max) noexcept;

  ossia::value value;
  State::Domain domain;

  std::optional<ossia::access_mode> ioType;
  ossia::bounding_mode clipMode{};
  ossia::repetition_filter repetitionFilter{};

  ossia::any_map extendedAttributes;

  State::AddressAccessor address;
};
}

JSON_METADATA(Device::AddressSettings, "AddressSettings")
Q_DECLARE_METATYPE(Device::AddressSettings)
Q_DECLARE_METATYPE(Device::FullAddressSettings)
Q_DECLARE_METATYPE(Device::FullAddressAccessorSettings)

W_REGISTER_ARGTYPE(Device::AddressSettings)
W_REGISTER_ARGTYPE(Device::FullAddressSettings)
W_REGISTER_ARGTYPE(Device::FullAddressAccessorSettings)
