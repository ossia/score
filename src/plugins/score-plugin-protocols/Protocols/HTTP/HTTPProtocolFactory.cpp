// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "HTTPProtocolFactory.hpp"

#include "HTTPDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>

#include <Protocols/HTTP/HTTPProtocolSettingsWidget.hpp>
#include <Protocols/HTTP/HTTPSpecificSettings.hpp>
#include <Protocols/LibraryDeviceEnumerator.hpp>

#include <ossia/network/base/device.hpp>

#include <QUrl>
namespace Protocols
{
QString HTTPProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("HTTP");
}

QString HTTPProtocolFactory::category() const noexcept
{
  return StandardCategories::web;
}

QUrl HTTPProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/http-device.html");
}

Device::DeviceEnumerators
HTTPProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  auto library_enumerator = new LibraryDeviceEnumerator{
      "Ossia.Http",
      {"qml"},
      HTTPProtocolFactory::static_concreteKey(),
      [](const QByteArray& arr) {
    return QVariant::fromValue(HTTPSpecificSettings{arr});
      },
      ctx};

  return {{"Library", library_enumerator}};
}

Device::DeviceInterface* HTTPProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new HTTPDevice{settings};
}

const Device::DeviceSettings& HTTPProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "HTTP";
    HTTPSpecificSettings specif;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* HTTPProtocolFactory::makeSettingsWidget()
{
  return new HTTPProtocolSettingsWidget;
}

QVariant
HTTPProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<HTTPSpecificSettings>(visitor);
}

void HTTPProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<HTTPSpecificSettings>(data, visitor);
}

bool HTTPProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  return true;
}
}
