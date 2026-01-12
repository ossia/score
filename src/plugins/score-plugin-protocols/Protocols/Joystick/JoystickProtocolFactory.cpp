
#include "JoystickProtocolFactory.hpp"

#include "JoystickDevice.hpp"
#include "JoystickProtocolSettingsWidget.hpp"
#include "JoystickSpecificSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <ossia/protocols/joystick/joystick_protocol.hpp>

#include <QObject>
#include <QUrl>
namespace Protocols
{
class DefaultJoystickEnumerator : public Device::DeviceEnumerator
{
public:
  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
    Device::DeviceSettings s;
    s.name = "Gamepad";
    s.protocol = JoystickProtocolFactory::static_concreteKey();
    JoystickSpecificSettings specif;
    specif.gamepad = true;
    specif.id = {};
    specif.spec = specif.unassigned;

    s.deviceSpecificSettings = QVariant::fromValue(specif);
    f("Default Gamepad", s);
  }
};

class JoystickEnumerator : public Device::DeviceEnumerator
{
public:
  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
    using info = ossia::net::joystick_info;
    const unsigned int joystick_count = info::get_joystick_count();

    for(unsigned int i = 0; i < joystick_count; ++i)
    {
      const char* s = info::get_joystick_name(i);
      if(s)
      {
        Device::DeviceSettings set;
        set.name = s;
        set.protocol = JoystickProtocolFactory::static_concreteKey();
        JoystickSpecificSettings specif;
        info::write_joystick_uuid(i, specif.id.data);
        specif.spec = {info::get_joystick_id(i), i};
        specif.gamepad = info::get_joystick_is_gamepad(i);

        set.deviceSpecificSettings = QVariant::fromValue(specif);
        f(set.name, set);
      }
    }
  }
};

QString JoystickProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Joystick");
}

QString JoystickProtocolFactory::category() const noexcept
{
  return StandardCategories::hardware;
}

QUrl JoystickProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/joystick-device.html");
}

Device::DeviceEnumerators
JoystickProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  return {
      {"Default", new DefaultJoystickEnumerator}, {"Devices", new JoystickEnumerator}};
}

Device::DeviceInterface* JoystickProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new JoystickDevice{settings, plugin.networkContext()};
}

const Device::DeviceSettings& JoystickProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings& settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Joystick";

    JoystickSpecificSettings settings;
    settings.id = {};
    settings.spec = {-1, -1};
    s.deviceSpecificSettings = QVariant::fromValue(settings);
    return s;
  }();

  return settings;
}

Device::ProtocolSettingsWidget* JoystickProtocolFactory::makeSettingsWidget()
{
  return new JoystickProtocolSettingsWidget;
}

QVariant JoystickProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<JoystickSpecificSettings>(visitor);
}

void JoystickProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<JoystickSpecificSettings>(data, visitor);
}

bool JoystickProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  auto a_ = a.deviceSpecificSettings.value<JoystickSpecificSettings>();
  if(a.protocol != b.protocol)
  {
    // FIXME check that the joystick is not already open
    return true;
  }

  auto b_ = b.deviceSpecificSettings.value<JoystickSpecificSettings>();
  return a_.id != b_.id || a_.spec != b_.spec;
}
}
