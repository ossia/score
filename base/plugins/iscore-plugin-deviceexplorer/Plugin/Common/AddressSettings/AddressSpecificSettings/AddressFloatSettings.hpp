#pragma once

#include <QString>
#include <QMetaType>

struct AddressFloatSettings
{
    float min{0.f};
    float max{1.f};
    QString unit{""};
    QString clipMode{""};
};

Q_DECLARE_METATYPE(AddressFloatSettings)
