#pragma once
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <Device/Address/IOType.hpp>
#include <Device/Address/ClipMode.hpp>
#include <Device/Address/Domain.hpp>
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
struct ISCORE_LIB_DEVICE_EXPORT AddressSettingsCommon
{
        AddressSettingsCommon();
        AddressSettingsCommon(const AddressSettingsCommon& );
        AddressSettingsCommon(AddressSettingsCommon&& );
        AddressSettingsCommon& operator=(const AddressSettingsCommon& );
        AddressSettingsCommon& operator=(AddressSettingsCommon&& );
        ~AddressSettingsCommon();

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
struct ISCORE_LIB_DEVICE_EXPORT AddressSettings : public Device::AddressSettingsCommon
{
        AddressSettings();
        AddressSettings(const AddressSettings& );
        AddressSettings(AddressSettings&& );
        AddressSettings& operator=(const AddressSettings& );
        AddressSettings& operator=(AddressSettings&& );
        ~AddressSettings();

        QString name;
};

// This one has the whole path of the node in address
struct ISCORE_LIB_DEVICE_EXPORT FullAddressSettings : public Device::AddressSettingsCommon
{
        FullAddressSettings();
        FullAddressSettings(const FullAddressSettings& );
        FullAddressSettings(FullAddressSettings&& );
        FullAddressSettings& operator=(const FullAddressSettings& );
        FullAddressSettings& operator=(FullAddressSettings&& );
        ~FullAddressSettings();

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

ISCORE_LIB_DEVICE_EXPORT bool operator==(
        const Device::AddressSettingsCommon& lhs,
        const Device::AddressSettingsCommon& rhs);

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

struct ISCORE_LIB_DEVICE_EXPORT FullAddressAccessorSettings
{
        FullAddressAccessorSettings();
        FullAddressAccessorSettings(const FullAddressAccessorSettings& );
        FullAddressAccessorSettings(FullAddressAccessorSettings&& );
        FullAddressAccessorSettings& operator=(const FullAddressAccessorSettings& );
        FullAddressAccessorSettings& operator=(FullAddressAccessorSettings&& );
        ~FullAddressAccessorSettings();

        FullAddressAccessorSettings(
                const State::AddressAccessor& addr,
                const AddressSettingsCommon& f);

        FullAddressAccessorSettings(
                State::AddressAccessor&& addr,
                AddressSettingsCommon&& f);

        FullAddressAccessorSettings(
                const State::AddressAccessor& addr,
                const ossia::value& min,
                const ossia::value& max);

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
