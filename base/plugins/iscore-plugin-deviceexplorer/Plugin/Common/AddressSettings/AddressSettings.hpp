#pragma once

#include <QString>
#include <QVariant>

// TODO there are two string maps, one with <-> and one with In/Out... collapse them ?
enum class IOType {Invalid, In, Out, InOut};
const QMap<IOType, QString>& IOTypeStringMap();

struct AddressSettings
{
    QString name;
    int priority{};
    QString tags; // TODO QStringList ?
    IOType ioType{};
    QVariant addressSpecificSettings;
    QVariant value;
};

// This one has the whole path of the node in the name
using FullAddressSettings = AddressSettings;
