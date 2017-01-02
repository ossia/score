#pragma once
#include <QString>
#include <QVariant>
#include <iscore/plugins/customfactory/UuidKey.hpp>
#include <iscore/tools/Metadata.hpp>
#include <iscore/tools/Todo.hpp>
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
  return lhs.protocol == rhs.protocol
      && lhs.name == rhs.name
      && lhs.deviceSpecificSettings == rhs.deviceSpecificSettings;
}
}

JSON_METADATA(Device::DeviceSettings, "DeviceSettings")
