
#include "JoystickProtocolFactory.hpp"

#include "JoystickDevice.hpp"
#include "JoystickProtocolSettingsWidget.hpp"
#include "JoystickSpecificSettings.hpp"

#include <QObject>

namespace Protocols
{

QString JoystickProtocolFactory::prettyName() const
{
  return QObject::tr("Joystick");
}

Device::DeviceInterface* JoystickProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const score::DocumentContext& ctx)
{
  return new JoystickDevice{settings};
}

const Device::DeviceSettings& JoystickProtocolFactory::defaultSettings() const
{
  static const Device::DeviceSettings& settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Joystick";
    JoystickSpecificSettings settings;
    s.deviceSpecificSettings = QVariant::fromValue(settings);
    return s;
  }();

  return settings;
}

Device::ProtocolSettingsWidget* JoystickProtocolFactory::makeSettingsWidget()
{
  return new JoystickProtocolSettingsWidget;
}

QVariant JoystickProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<JoystickSpecificSettings>(visitor);
}

void JoystickProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<JoystickSpecificSettings>(data, visitor);
}

bool JoystickProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const
{
  auto a_ = a.deviceSpecificSettings.value<JoystickSpecificSettings>();
  auto b_ = b.deviceSpecificSettings.value<JoystickSpecificSettings>();
  return a_.joystick_id != b_.joystick_id;
}
}
