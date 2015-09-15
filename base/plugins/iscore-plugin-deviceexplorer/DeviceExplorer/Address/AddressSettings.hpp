#pragma once
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <State/Address.hpp>
#include <State/Value.hpp>
#include "IOType.hpp"
#include "ClipMode.hpp"
#include "Domain.hpp"
namespace iscore
{
using RefreshRate = int;
using RepetitionFilter = bool;

struct AddressSettingsCommon
{
    iscore::Value value;
    iscore::Domain domain;

    iscore::IOType ioType{};
    iscore::ClipMode clipMode{};
    QString unit;

    iscore::RepetitionFilter repetitionFilter{};
    iscore::RefreshRate rate{};

    int priority{};

    QStringList tags;
};

// this one has only the name of the current node (e.g. 'a' for dev:/azazd/a)
struct AddressSettings : public iscore::AddressSettingsCommon
{
        QString name;
};

// This one has the whole path of the node in address
struct FullAddressSettings : public iscore::AddressSettingsCommon
{
        struct as_parent;
        struct as_child;
        iscore::Address address;

        template<typename T>
        static FullAddressSettings make(
                const iscore::AddressSettings& other,
                const iscore::Address& addr);
        // Specializations are in FullAddressSettings.cpp
};

inline bool operator==(
        const iscore::AddressSettings& lhs,
        const iscore::AddressSettings& rhs)
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
        const iscore::AddressSettings& lhs,
        const iscore::AddressSettings& rhs)
{
    return !(lhs == rhs);
}
}
