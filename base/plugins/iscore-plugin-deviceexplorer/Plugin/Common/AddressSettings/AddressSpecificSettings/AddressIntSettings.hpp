#pragma once

#include <QString>
#include <QMetaType>

struct AddressIntSettings
{
    int min{0};
    int max{0};
    QString clipMode{""};
    QString unit{""};
};
Q_DECLARE_METATYPE(AddressIntSettings)
