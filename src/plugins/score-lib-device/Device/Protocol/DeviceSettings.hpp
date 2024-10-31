#pragma once
#include <score/plugins/UuidKey.hpp>
#include <score/serialization/DataStreamFwd.hpp>
#include <score/tools/Metadata.hpp>

#include <ossia/detail/small_vector.hpp>
#include <ossia/detail/variant.hpp>

#include <QString>
#include <QVariant>

#include <score_lib_device_export.h>

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

inline bool operator==(const DeviceSettings& lhs, const DeviceSettings& rhs) noexcept
{
  return lhs.protocol == rhs.protocol && lhs.name == rhs.name
         && lhs.deviceSpecificSettings == rhs.deviceSpecificSettings;
}

struct UDPPortDeviceResource
{
  int port{};
};
struct TCPPortDeviceResource
{
  int port{};
};
struct HardwarePortDeviceResource
{
  QString hardware;
};

using DeviceResource = ossia::variant<
    UDPPortDeviceResource, TCPPortDeviceResource, HardwarePortDeviceResource>;
using DeviceResources = ossia::small_vector<DeviceResource, 2>;
using DeviceResourceMap = ossia::flat_map<QString, DeviceResources>;
}

// See note in AddressSettings.hpp for Address / Device
JSON_METADATA(Device::DeviceSettings, "Device")
SCORE_SERIALIZE_DATASTREAM_DECLARE(SCORE_LIB_DEVICE_EXPORT, Device::DeviceSettings);

Q_DECLARE_METATYPE(Device::DeviceSettings)
W_REGISTER_ARGTYPE(Device::DeviceSettings)
