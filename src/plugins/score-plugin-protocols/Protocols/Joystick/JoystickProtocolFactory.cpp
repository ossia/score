
#include "JoystickProtocolFactory.hpp"

#include "JoystickDevice.hpp"
#include "JoystickProtocolSettingsWidget.hpp"
#include "JoystickSpecificSettings.hpp"
#include <ossia/protocols/joystick/joystick_protocol.hpp>

#include <QObject>
namespace Protocols
{

class JoystickEnumerator : public Device::DeviceEnumerator
{
public:
  void enumerate(std::function<void(const Device::DeviceSettings&)> f) const override
  {
    const unsigned int joystick_count = ossia::net::joystick_info::get_joystick_count();

    for (unsigned int i = 0; i < joystick_count; ++i)
    {
      const char* s = ossia::net::joystick_info::get_joystick_name(i);
      if (s)
      {
        Device::DeviceSettings set;
        set.name = s;
        set.protocol = JoystickProtocolFactory::static_concreteKey();
        JoystickSpecificSettings specif;
        ossia::net::joystick_info::write_joystick_uuid(i, specif.id.data);
        specif.spec = {ossia::net::joystick_info::get_joystick_id(i), i};

        set.deviceSpecificSettings = QVariant::fromValue(specif);
        f(set);
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

Device::DeviceEnumerator* JoystickProtocolFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return new JoystickEnumerator;
}

Device::DeviceInterface* JoystickProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const score::DocumentContext& ctx)
{
  return new JoystickDevice{settings, ctx};
}

const Device::DeviceSettings& JoystickProtocolFactory::defaultSettings() const noexcept
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
    const Device::DeviceSettings& b) const noexcept
{
  auto a_ = a.deviceSpecificSettings.value<JoystickSpecificSettings>();
  auto b_ = b.deviceSpecificSettings.value<JoystickSpecificSettings>();
  return a_.id != b_.id || a_.spec != b_.spec;
}
}
