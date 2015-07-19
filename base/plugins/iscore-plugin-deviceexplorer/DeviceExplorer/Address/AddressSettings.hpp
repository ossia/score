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
        iscore::Address address;

        // Second argument should be the address of the parent.
        static FullAddressSettings make(
                const iscore::AddressSettings& other,
                const iscore::Address& addr)
        {
            FullAddressSettings as;
            static_cast<AddressSettingsCommon&>(as) = other;

            as.address = addr;
            as.address.path.append(other.name);

            return as;
        }
};

}
