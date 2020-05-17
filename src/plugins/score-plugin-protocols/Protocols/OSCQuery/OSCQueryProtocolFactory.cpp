// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OSCQueryProtocolFactory.hpp"

#include "OSCQueryDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>
#include <Protocols/OSCQuery/OSCQueryProtocolSettingsWidget.hpp>
#include <Protocols/OSCQuery/OSCQuerySpecificSettings.hpp>

#include <ossia/network/base/device.hpp>

#include <QObject>

namespace Protocols
{
QString OSCQueryProtocolFactory::prettyName() const
{
  return QObject::tr("OSCQuery");
}

int OSCQueryProtocolFactory::visualPriority() const
{
  return 2;
}

Device::DeviceInterface* OSCQueryProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const score::DocumentContext& ctx)
{
  return new OSCQueryDevice{settings};
}

const Device::DeviceSettings& OSCQueryProtocolFactory::defaultSettings() const
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "OSCQuery";
    OSCQuerySpecificSettings specif;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* OSCQueryProtocolFactory::makeSettingsWidget()
{
  return new OSCQueryProtocolSettingsWidget;
}

QVariant OSCQueryProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<OSCQuerySpecificSettings>(visitor);
}

void OSCQueryProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<OSCQuerySpecificSettings>(data, visitor);
}

bool OSCQueryProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const
{
  return a.name != b.name;
}
}
