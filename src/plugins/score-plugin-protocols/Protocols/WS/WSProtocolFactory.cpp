// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "WSProtocolFactory.hpp"

#include "WSDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>
#include <Protocols/WS/WSProtocolSettingsWidget.hpp>
#include <Protocols/WS/WSSpecificSettings.hpp>

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
QString WSProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("WS");
}
QString WSProtocolFactory::category() const noexcept
{
  return StandardCategories::web;
}
Device::DeviceEnumerator* WSProtocolFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return nullptr;
}

Device::DeviceInterface* WSProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const score::DocumentContext& ctx)
{
  return new WSDevice{settings};
}

const Device::DeviceSettings& WSProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
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

QVariant WSProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<WSSpecificSettings>(visitor);
}

void WSProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<WSSpecificSettings>(data, visitor);
}

bool WSProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  return a.name != b.name;
}
}
