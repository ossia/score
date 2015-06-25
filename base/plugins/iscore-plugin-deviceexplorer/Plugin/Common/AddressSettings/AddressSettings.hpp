#pragma once

#include <QString>
#include <QVariant>
#include <QVariantList>

// TODO namespace
enum class IOType { Invalid, In, Out, InOut };
const QMap<IOType, QString>& IOTypeStringMap();

enum class ClipMode { Clip, Fold, Free, Wrap };
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

struct AddressSettings
{
    QString name;

    QVariant value;
    iscore::Domain domain;

    IOType ioType{};
    ClipMode clipMode{};
    QString unit;

    RepetitionFilter repetitionFilter{};
    RefreshRate rate{};

    int priority{};

    QStringList tags;
};

// This one has the whole path of the node in the name
using FullAddressSettings = AddressSettings;
