#pragma once
#include <QString>
#include <QVariant>
#include <iscore/tools/Todo.hpp>
#include <iscore/plugins/customfactory/UuidKey.hpp>
namespace Device {
class ProtocolFactory;
struct DeviceSettings
{
        UuidKey<Device::ProtocolFactory> protocol;
        QString name;
        QVariant deviceSpecificSettings;
};
}
