// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "HTTPProtocolFactory.hpp"

#include "HTTPDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>
#include <Protocols/HTTP/HTTPProtocolSettingsWidget.hpp>
#include <Protocols/HTTP/HTTPSpecificSettings.hpp>

#include <ossia/network/base/device.hpp>

#include <QObject>

namespace Device
{
class DeviceInterface;
class ProtocolSettingsWidget;
}

struct VisitorVariant;

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

Device::DeviceEnumerator* HTTPProtocolFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return nullptr;
}


Device::DeviceInterface* HTTPProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
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

QVariant HTTPProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<HTTPSpecificSettings>(visitor);
}

void HTTPProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<HTTPSpecificSettings>(data, visitor);
}

bool HTTPProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  return a.name != b.name;
}
}
