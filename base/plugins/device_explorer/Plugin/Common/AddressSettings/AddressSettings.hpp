#pragma once

#include <QString>
#include <QVariant>

struct AddressSettings
{
    QString name;
    QString valueType; // TODO why not an enum?
    int priority;
    QString tags;
    QString ioType; // TODO why not an enum?
    QVariant addressSpecificSettings;
    QVariant value;
};

// This one has the whole path of the node in the name
using FullAddressSettings = AddressSettings;
