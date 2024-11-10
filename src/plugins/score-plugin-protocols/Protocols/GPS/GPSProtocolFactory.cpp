#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_GPS)
#include "GPSDevice.hpp"
#include "GPSProtocolFactory.hpp"
#include "GPSProtocolSettingsWidget.hpp"
#include "GPSSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QObject>
#include <QUrl>

namespace Protocols
{

QString GPSProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("GPS");
}

QString GPSProtocolFactory::category() const noexcept
{
  return StandardCategories::hardware;
}

QUrl GPSProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/gps-device.html");
}

Device::DeviceInterface* GPSProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new GPSDevice{settings, plugin.networkContext()};
}

const Device::DeviceSettings& GPSProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings& settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "gps";
    GPSSpecificSettings settings;
    settings.host = "127.0.0.1";
    settings.port = 2947;
    s.deviceSpecificSettings = QVariant::fromValue(settings);
    return s;
  }();

  return settings;
}

Device::ProtocolSettingsWidget* GPSProtocolFactory::makeSettingsWidget()
{
  return new GPSProtocolSettingsWidget;
}

QVariant
GPSProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<GPSSpecificSettings>(visitor);
}

void GPSProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<GPSSpecificSettings>(data, visitor);
}

bool GPSProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  return true; //  TODO
}
}
#endif
