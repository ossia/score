#pragma once

#include <QString>
#include <QVariant>
#include <QVariantList>
#include <State/Address.hpp>
#include <State/Value.hpp>

// TODO namespace
enum class IOType : int { Invalid, In, Out, InOut };
const QMap<IOType, QString>& IOTypeStringMap();

enum class ClipMode : int { Clip, Fold, Free, Wrap };
const QMap<ClipMode, QString> &ClipModeStringMap();

namespace iscore
{
struct Domain
{
        QVariant min;
        QVariant max;
        QVariantList values;
};
}

using RefreshRate = int;
using RepetitionFilter = bool;

struct AddressSettingsCommon
{
    iscore::Value value;
    iscore::Domain domain;

    IOType ioType{};
    ClipMode clipMode{};
    QString unit;

    RepetitionFilter repetitionFilter{};
    RefreshRate rate{};

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
        static FullAddressSettings make(const AddressSettingsCommon& other)
        {
            FullAddressSettings as;
            static_cast<AddressSettingsCommon&>(as) = other;
            return as;
        }
};

