#pragma once
#include <score/plugins/customfactory/UuidKey.hpp>
#include <score/tools/Metadata.hpp>
#include <score/tools/Todo.hpp>

#include <QString>
#include <QVariant>
namespace Device
{
class ProtocolFactory;
struct DeviceSettings
{
  UuidKey<Device::ProtocolFactory> protocol;
  QString name;
  QVariant deviceSpecificSettings;
};

inline bool operator==(const DeviceSettings& lhs, const DeviceSettings& rhs)
{
  return lhs.protocol == rhs.protocol && lhs.name == rhs.name
         && lhs.deviceSpecificSettings == rhs.deviceSpecificSettings;
}
}

JSON_METADATA(Device::DeviceSettings, "DeviceSettings")
