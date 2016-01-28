#pragma once
#include <QString>
#include <QVariant>
#include <iscore/tools/Todo.hpp>

#include <Device/Protocol/ProtocolFactoryKey.hpp>

namespace Device {
struct DeviceSettings
{
        ProtocolFactoryKey protocol;
        QString name;
        QVariant deviceSpecificSettings;
};
}
