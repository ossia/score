#pragma once
#include <QString>
#include <QVariant>
#include <QVariantList>
#include "IOType.hpp"
#include "ClipMode.hpp"
#include "Domain.hpp"
#include <State/Message.hpp>
#include <State/Unit.hpp>
#include <iscore/tools/Metadata.hpp>
#include <iscore_lib_device_export.h>

template<typename T>
class TreeNode;
namespace Device
{
class DeviceExplorerNode;
using Node = TreeNode<DeviceExplorerNode>;

using RefreshRate = int;
using RepetitionFilter = bool;
struct AddressSettingsCommon
{
    State::Value value;
    ossia::net::domain domain;

    Device::IOType ioType{};
    Device::ClipMode clipMode{};
    ossia::unit_t unit;

    Device::RepetitionFilter repetitionFilter{};
    Device::RefreshRate rate{};

    int priority{};

    QStringList tags;
    QString description;
};

// this one has only the name of the current node (e.g. 'a' for dev:/azazd/a)
struct AddressSettings : public Device::AddressSettingsCommon
{
        QString name;
};

// This one has the whole path of the node in address
struct FullAddressSettings : public Device::AddressSettingsCommon
{
        // Maybe we should just use FullAddressSettings behind a flyweight
        // pattern everywhere... (see mnmlstc/flyweight)
        struct as_parent;
        struct as_child;
        State::Address address;

        template<typename T>
        ISCORE_LIB_DEVICE_EXPORT static FullAddressSettings make(
                const Device::AddressSettings& other,
                const State::Address& addr);
        template<typename T>
        static FullAddressSettings make(
                const Device::AddressSettings& other,
                const State::AddressAccessor& addr)
        { return make<T>(other, addr.address); }

        ISCORE_LIB_DEVICE_EXPORT static FullAddressSettings make(
                const State::Message& mess);

        ISCORE_LIB_DEVICE_EXPORT static FullAddressSettings make(
                const Device::Node& node);
        // Specializations are in FullAddressSettings.cpp
};

inline bool operator==(
        const Device::AddressSettingsCommon& lhs,
        const Device::AddressSettingsCommon& rhs)
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

inline bool operator!=(
        const Device::AddressSettingsCommon& lhs,
        const Device::AddressSettingsCommon& rhs)
{
    return !(lhs == rhs);
}
inline bool operator==(
        const Device::AddressSettings& lhs,
        const Device::AddressSettings& rhs)
{
    return static_cast<const Device::AddressSettingsCommon&>(lhs) == static_cast<const Device::AddressSettingsCommon&>(rhs)
            && lhs.name == rhs.name;
}

inline bool operator!=(
        const Device::AddressSettings& lhs,
        const Device::AddressSettings& rhs)
{
    return !(lhs == rhs);
}
inline bool operator==(
        const Device::FullAddressSettings& lhs,
        const Device::FullAddressSettings& rhs)
{
    return static_cast<const Device::AddressSettingsCommon&>(lhs) == static_cast<const Device::AddressSettingsCommon&>(rhs)
            && lhs.address == rhs.address;
}

inline bool operator!=(
        const Device::FullAddressSettings& lhs,
        const Device::FullAddressSettings& rhs)
{
    return !(lhs == rhs);
}

struct FullAddressAccessorSettings
{
  State::Value value;
  ossia::net::domain domain;

  Device::IOType ioType{};
  Device::ClipMode clipMode{};

  Device::RepetitionFilter repetitionFilter{};
  Device::RefreshRate rate{};

  int priority{};

  QStringList tags;
  QString description;

  State::AddressAccessor address;

  FullAddressAccessorSettings() = default;
  FullAddressAccessorSettings(const FullAddressAccessorSettings&) = default;
  FullAddressAccessorSettings(FullAddressAccessorSettings&&) = default;
  FullAddressAccessorSettings& operator=(const FullAddressAccessorSettings&) = default;
  FullAddressAccessorSettings& operator=(FullAddressAccessorSettings&&) = default;

  FullAddressAccessorSettings(
      const State::AddressAccessor& addr,
      const AddressSettingsCommon& f):
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

  FullAddressAccessorSettings(
      State::AddressAccessor&& addr,
      AddressSettingsCommon&& f):
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

  FullAddressAccessorSettings(
      const State::AddressAccessor& addr,
      const ossia::value& min,
      const ossia::value& max):
      domain{ossia::net::make_domain(min, max)},
      address{addr}
  {

  }

};
}

JSON_METADATA(Device::AddressSettings, "AddressSettings")
Q_DECLARE_METATYPE(Device::AddressSettings)
Q_DECLARE_METATYPE(Device::FullAddressSettings)
Q_DECLARE_METATYPE(Device::FullAddressAccessorSettings)
