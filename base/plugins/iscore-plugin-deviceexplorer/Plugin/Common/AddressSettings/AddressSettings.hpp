#pragma once

#include <QString>
#include <QVariant>

enum class IOType {Invalid, In, Out, InOut};
const QMap<IOType, QString>& IOTypeStringMap();

struct AddressSettings
{
    QString name;
    QString valueType; // TODO why not an enum?
    int priority{};
    QString tags;
    IOType ioType{}; // TODO why not an enum?
    QVariant addressSpecificSettings;
    QVariant value;
};

// This one has the whole path of the node in the name
using FullAddressSettings = AddressSettings;
