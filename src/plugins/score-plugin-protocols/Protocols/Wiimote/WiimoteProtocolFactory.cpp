
#include "WiimoteProtocolFactory.hpp"

#include "WiimoteDevice.hpp"
#include "WiimoteProtocolSettingsWidget.hpp"
#include "WiimoteSpecificSettings.hpp"

#include <QObject>

namespace Protocols
{

QString WiimoteProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Wiimote");
}

QString WiimoteProtocolFactory::category() const noexcept
{
  return StandardCategories::hardware;
}

Device::DeviceEnumerator* WiimoteProtocolFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return nullptr;
}


Device::DeviceInterface* WiimoteProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const score::DocumentContext& ctx)
{
  return new WiimoteDevice{settings, ctx};
}

const Device::DeviceSettings& WiimoteProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings& settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Wiimote";
    WiimoteSpecificSettings settings;
    s.deviceSpecificSettings = QVariant::fromValue(settings);
    return s;
  }();

  return settings;
}

Device::ProtocolSettingsWidget* WiimoteProtocolFactory::makeSettingsWidget()
{
  return new WiimoteProtocolSettingsWidget;
}

QVariant WiimoteProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<WiimoteSpecificSettings>(visitor);
}

void WiimoteProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<WiimoteSpecificSettings>(data, visitor);
}

bool WiimoteProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  return false;
}

}
