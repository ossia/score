#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include "ArtnetDevice.hpp"
#include "ArtnetProtocolFactory.hpp"
#include "ArtnetProtocolSettingsWidget.hpp"
#include "ArtnetSpecificSettings.hpp"

#include <QObject>

namespace Protocols
{

QString ArtnetProtocolFactory::prettyName() const
{
  return QObject::tr("Artnet");
}

Device::DeviceInterface* ArtnetProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const score::DocumentContext& ctx)
{
  return new ArtnetDevice{settings};
}

const Device::DeviceSettings& ArtnetProtocolFactory::defaultSettings() const
{
  static const Device::DeviceSettings& settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Artnet";
    ArtnetSpecificSettings settings;
    s.deviceSpecificSettings = QVariant::fromValue(settings);
    return s;
  }();

  return settings;
}

Device::ProtocolSettingsWidget* ArtnetProtocolFactory::makeSettingsWidget()
{
  return new ArtnetProtocolSettingsWidget;
}

QVariant ArtnetProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<ArtnetSpecificSettings>(visitor);
}

void ArtnetProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<ArtnetSpecificSettings>(data, visitor);
}

bool ArtnetProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const
{
  return false; //  TODO
}
}
#endif
