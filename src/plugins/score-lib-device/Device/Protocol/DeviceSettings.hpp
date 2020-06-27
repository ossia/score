#pragma once
#include <score/plugins/UuidKey.hpp>
#include <score/tools/Metadata.hpp>

#include <QString>
#include <QVariant>
#include <verdigris>
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

// See note in AddressSettings.hpp for Address / Device
JSON_METADATA(Device::DeviceSettings, "Device")
Q_DECLARE_METATYPE(Device::DeviceSettings)
W_REGISTER_ARGTYPE(Device::DeviceSettings)
