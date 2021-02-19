// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OSCProtocolFactory.hpp"

#include "OSCDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>
#include <Protocols/LibraryDeviceEnumerator.hpp>
#include <Protocols/OSC/OSCProtocolSettingsWidget.hpp>
#include <Protocols/OSC/OSCSpecificSettings.hpp>

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
QString OSCProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("OSC");
}

QString OSCProtocolFactory::category() const noexcept
{
  return StandardCategories::osc;
}

Device::DeviceEnumerator* OSCProtocolFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return new LibraryDeviceEnumerator{
    "9a42de4b-f6eb-4bca-9564-01b975f601b9",
    "json",
    OSCProtocolFactory::static_concreteKey(),
        [] (const QByteArray& arr) {
      return QVariant::fromValue(OSCSpecificSettings{9996, 9997, "127.0.0.1", std::nullopt, arr});
    },
    ctx};
}

Device::DeviceInterface* OSCProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const score::DocumentContext& ctx)
{
  return new OSCDevice{settings, ctx};
}

const Device::DeviceSettings& OSCProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "OSC";
    OSCSpecificSettings specif;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* OSCProtocolFactory::makeSettingsWidget()
{
  return new OSCProtocolSettingsWidget;
}

QVariant OSCProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<OSCSpecificSettings>(visitor);
}

void OSCProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<OSCSpecificSettings>(data, visitor);
}

bool OSCProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  auto a_p = a.deviceSpecificSettings.value<OSCSpecificSettings>();
  auto b_p = b.deviceSpecificSettings.value<OSCSpecificSettings>();
  return a.name != b.name && a_p.deviceListeningPort != b_p.deviceListeningPort && a_p.scoreListeningPort != b_p.scoreListeningPort;
}
}
