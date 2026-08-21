#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_CAN)
#include "CANDevice.hpp"
#include "CANProtocolFactory.hpp"
#include "CANProtocolSettingsWidget.hpp"
#include "CANSpecificSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <QObject>
#include <QUrl>

namespace Protocols
{

QString CANProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("CAN");
}

QString CANProtocolFactory::category() const noexcept
{
  return StandardCategories::hardware;
}

QUrl CANProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/can-device.html");
}

Device::DeviceInterface* CANProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new CANDevice{settings, plugin.networkContext()};
}

const Device::DeviceSettings& CANProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings& settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "CAN";
    CANSpecificSettings settings;
    settings.interfaceName = "can0";
    s.deviceSpecificSettings = QVariant::fromValue(settings);
    return s;
  }();

  return settings;
}

Device::ProtocolSettingsWidget* CANProtocolFactory::makeSettingsWidget()
{
  return new CANProtocolSettingsWidget;
}

QVariant
CANProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<CANSpecificSettings>(visitor);
}

void CANProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<CANSpecificSettings>(data, visitor);
}

bool CANProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  auto lhs = a.deviceSpecificSettings.value<CANSpecificSettings>();
  auto rhs = b.deviceSpecificSettings.value<CANSpecificSettings>();

  // Several devices on one interface is the normal case, not a conflict: a
  // chain of sensors is one DBC file opened N times with N node-id offsets,
  // and SocketCAN gives each socket its own view of the bus. Only two devices
  // that would decode the very same frames are actually redundant.
  if(lhs.interfaceName != rhs.interfaceName)
    return true;

  return lhs.nodeIdOffset != rhs.nodeIdOffset || lhs.dbcPath != rhs.dbcPath;
}
}
#endif
