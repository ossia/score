// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SERIAL)
#include "SerialProtocolFactory.hpp"

#include "SerialDevice.hpp"

#include <ossia/network/base/device.hpp>

#include <Device/Protocol/DeviceSettings.hpp>
#include <Engine/Protocols/Serial/SerialProtocolSettingsWidget.hpp>
#include <Engine/Protocols/Serial/SerialSpecificSettings.hpp>
#include <QObject>

namespace Device
{
class DeviceInterface;
class ProtocolSettingsWidget;
}

struct VisitorVariant;

namespace Engine
{
namespace Network
{
QString SerialProtocolFactory::prettyName() const
{
  return QObject::tr("Serial");
}

Device::DeviceInterface* SerialProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const score::DocumentContext& ctx)
{
  return new SerialDevice{settings};
}

const Device::DeviceSettings& SerialProtocolFactory::defaultSettings() const
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Serial";
    SerialSpecificSettings specif;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* SerialProtocolFactory::makeSettingsWidget()
{
  return new SerialProtocolSettingsWidget;
}

QVariant SerialProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<SerialSpecificSettings>(visitor);
}

void SerialProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<SerialSpecificSettings>(data, visitor);
}

bool SerialProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const
{
  return a.name != b.name;
}
}
}
#endif
