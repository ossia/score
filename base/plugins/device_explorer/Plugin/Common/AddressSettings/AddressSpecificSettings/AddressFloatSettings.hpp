#pragma once

#include <QString>
#include <QMetaType>

struct AddressFloatSettings
{
    float min;
    float max;
    QString unit;
    QString clipMode;
};

Q_DECLARE_METATYPE(AddressFloatSettings)
