#include <QObject>

#include "WSDevice.hpp"
#include "WSProtocolFactory.hpp"
#include <Device/Protocol/DeviceSettings.hpp>
#include <Engine/Protocols/WS/WSProtocolSettingsWidget.hpp>
#include <Engine/Protocols/WS/WSSpecificSettings.hpp>
#include <ossia/network/base/device.hpp>

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
QString WSProtocolFactory::prettyName() const
{
  return QObject::tr("WS");
}

Device::DeviceInterface* WSProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const iscore::DocumentContext& ctx)
{
  return new WSDevice{settings};
}

const Device::DeviceSettings& WSProtocolFactory::defaultSettings() const
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteFactoryKey();
    s.name = "WS";
    WSSpecificSettings specif;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* WSProtocolFactory::makeSettingsWidget()
{
  return new WSProtocolSettingsWidget;
}

QVariant WSProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<WSSpecificSettings>(visitor);
}

void WSProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<WSSpecificSettings>(data, visitor);
}

bool WSProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const
{
  return a.name != b.name;
}
}
}
