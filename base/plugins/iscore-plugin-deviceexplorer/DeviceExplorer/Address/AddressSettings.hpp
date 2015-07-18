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
}

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

struct AddressSettings : public AddressSettingsCommon
{
        QString name;
};

// This one has the whole path of the node in the name
struct FullAddressSettings : public AddressSettingsCommon
{
        iscore::Address address;
        static FullAddressSettings make(const AddressSettingsCommon& other) // TODO put it a qstringlist in args
        {
            FullAddressSettings as;
            static_cast<AddressSettingsCommon&>(as) = other;
            return as;
        }
};

