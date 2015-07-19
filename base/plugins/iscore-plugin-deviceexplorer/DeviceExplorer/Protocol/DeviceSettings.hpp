#pragma once
#include <QString>
#include <QVariant>

namespace iscore {
struct DeviceSettings
{
        QString protocol;
        QString name;
        QVariant deviceSpecificSettings;
};
}
