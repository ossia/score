#pragma once

#include <QString>
#include <QMetaType>

struct AddressIntSettings
{
    int min;
    int max;
    QString clipMode;
    QString unit;
};
Q_DECLARE_METATYPE(AddressIntSettings)
