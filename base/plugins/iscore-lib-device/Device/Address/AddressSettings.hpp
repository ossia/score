#pragma once
#include <Device/Address/ClipMode.hpp>
#include <Device/Address/Domain.hpp>
#include <Device/Address/IOType.hpp>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <State/Message.hpp>
#include <State/Unit.hpp>
#include <iscore/tools/Metadata.hpp>
#include <iscore_lib_device_export.h>

template <typename T>
class TreeNode;
namespace Device
{
class DeviceExplorerNode;

/** A data-only tree of nodes.
 *
 * By opposition to ossia::net::node_base, these nodes
 * contain pure data, no callbacks or complicated data structures.
 * They can be serialized very easily and are used as the data model of
 * Explorer::DeviceExplorerModel, as well as for serialization of devices.
 */
using Node = TreeNode<DeviceExplorerNode>;

using RefreshRate = int;
using RepetitionFilter = bool;
struct ISCORE_LIB_DEVICE_EXPORT AddressSettingsCommon
{
  AddressSettingsCommon() noexcept;
  AddressSettingsCommon(const AddressSettingsCommon&) noexcept;
  AddressSettingsCommon(AddressSettingsCommon&&) noexcept;
  AddressSettingsCommon& operator=(const AddressSettingsCommon&) noexcept;
  AddressSettingsCommon& operator=(AddressSettingsCommon&&) noexcept;
  ~AddressSettingsCommon() noexcept;

  State::Value value;
  Device::Domain domain;

  Device::IOType ioType{};
  Device::ClipMode clipMode{};
  State::Unit unit;

  Device::RepetitionFilter repetitionFilter{};
  Device::RefreshRate rate{};

  int priority{};

  QStringList tags;
  QString description;
};

// this one has only the name of the current node (e.g. 'a' for dev:/azazd/a)
struct ISCORE_LIB_DEVICE_EXPORT AddressSettings
    : public Device::AddressSettingsCommon
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
struct ISCORE_LIB_DEVICE_EXPORT FullAddressSettings
    : public Device::AddressSettingsCommon
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
  ISCORE_LIB_DEVICE_EXPORT static FullAddressSettings
  make(const Device::AddressSettings& other, const State::Address& addr) noexcept;
  template <typename T>
  static FullAddressSettings make(
      const Device::AddressSettings& other, const State::AddressAccessor& addr) noexcept
  {
    return make<T>(other, addr.address);
  }

  static FullAddressSettings
  make(const State::Message& mess) noexcept;

  static FullAddressSettings
  make(const Device::Node& node) noexcept;
  // Specializations are in FullAddressSettings.cpp
};

ISCORE_LIB_DEVICE_EXPORT bool operator==(
    const Device::AddressSettingsCommon& lhs,
    const Device::AddressSettingsCommon& rhs) noexcept;

inline bool operator!=(
    const Device::AddressSettingsCommon& lhs,
    const Device::AddressSettingsCommon& rhs) noexcept
{
  return !(lhs == rhs);
}
inline bool operator==(
    const Device::AddressSettings& lhs, const Device::AddressSettings& rhs) noexcept
{
  return static_cast<const Device::AddressSettingsCommon&>(lhs)
             == static_cast<const Device::AddressSettingsCommon&>(rhs)
         && lhs.name == rhs.name;
}

inline bool operator!=(
    const Device::AddressSettings& lhs, const Device::AddressSettings& rhs) noexcept
{
  return !(lhs == rhs);
}
inline bool operator==(
    const Device::FullAddressSettings& lhs,
    const Device::FullAddressSettings& rhs) noexcept
{
  return static_cast<const Device::AddressSettingsCommon&>(lhs)
             == static_cast<const Device::AddressSettingsCommon&>(rhs)
         && lhs.address == rhs.address;
}

inline bool operator!=(
    const Device::FullAddressSettings& lhs,
    const Device::FullAddressSettings& rhs) noexcept
{
  return !(lhs == rhs);
}

struct ISCORE_LIB_DEVICE_EXPORT FullAddressAccessorSettings
{
  FullAddressAccessorSettings() noexcept;
  FullAddressAccessorSettings(const FullAddressAccessorSettings&) noexcept;
  FullAddressAccessorSettings(FullAddressAccessorSettings&&) noexcept;
  FullAddressAccessorSettings&
  operator=(const FullAddressAccessorSettings&) noexcept;
  FullAddressAccessorSettings&
  operator=(FullAddressAccessorSettings&&) noexcept;
  ~FullAddressAccessorSettings() noexcept;

  FullAddressAccessorSettings(
      const State::AddressAccessor& addr, const AddressSettingsCommon& f) noexcept;

  explicit FullAddressAccessorSettings(
      FullAddressSettings&& f) noexcept;

  FullAddressAccessorSettings(
      State::AddressAccessor&& addr, AddressSettingsCommon&& f) noexcept;

  FullAddressAccessorSettings(
      const State::AddressAccessor& addr,
      const ossia::value& min,
      const ossia::value& max) noexcept;

  State::Value value;
  Device::Domain domain;

  Device::IOType ioType{};
  Device::ClipMode clipMode{};

  Device::RepetitionFilter repetitionFilter{};
  Device::RefreshRate rate{};

  int priority{};

  QStringList tags;
  QString description;

  State::AddressAccessor address;
};
}

JSON_METADATA(Device::AddressSettings, "AddressSettings")
Q_DECLARE_METATYPE(Device::AddressSettings)
Q_DECLARE_METATYPE(Device::FullAddressSettings)
Q_DECLARE_METATYPE(Device::FullAddressAccessorSettings)
