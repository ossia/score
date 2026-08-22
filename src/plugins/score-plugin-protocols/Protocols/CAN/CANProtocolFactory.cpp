#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_CAN)
#include "CANDevice.hpp"
#include "CANInterfaces.hpp"
#include "CANProtocolFactory.hpp"
#include "CANProtocolSettingsWidget.hpp"
#include "CANSpecificSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Protocols/LibraryDeviceEnumerator.hpp>

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

Device::DeviceEnumerators
CANProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  // The databases, not the interfaces: the settings widget's combo already
  // lists those, while a .dbc describes one device on the bus and is what gives
  // the device its tree. "BO_" is the message keyword - a database without one
  // has nothing to offer.
  auto library_enumerator = new LibraryDeviceEnumerator{
      "BO_",
      {"dbc"},
      CANProtocolFactory::static_concreteKey(),
      [](QByteArray, const QString& path) {
    CANSpecificSettings specif;
    specif.dbcPath = path;
    // So that a device dropped in from the library is connectable as-is,
    // rather than being a database with nowhere to read it from.
    specif.interfaceName = CAN::defaultInterface();
    return QVariant::fromValue(specif);
  },
      ctx};

  return {{"Library", library_enumerator}};
}

const Device::DeviceSettings& CANProtocolFactory::defaultSettings() const noexcept
{
  Device::DeviceSettings s;
  s.protocol = concreteKey();
  s.name = "CAN";

  CANSpecificSettings specif;
  // The first interface present, or nothing: hardcoding "can0" made every
  // fresh device fail with "no such CAN interface" on a machine without one.
  specif.interfaceName = CAN::defaultInterface();
  s.deviceSpecificSettings = QVariant::fromValue(specif);

  m_defaultSettings = std::move(s);
  return m_defaultSettings;
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
