#pragma once
#include <QString>
#include <QVariant>

struct DeviceSettings
{
        QString protocol;
        QString name;
        QVariant deviceSpecificSettings;
};
