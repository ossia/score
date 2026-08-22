#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_CAN)
#include "CANDevice.hpp"
#include "CANInterfaces.hpp"
#include "CANProtocolFactory.hpp"
#include "CANProtocolSettingsWidget.hpp"
#include "CANSpecificSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QObject>
#include <QTimer>
#include <QUrl>

namespace Protocols
{
namespace
{
Device::DeviceSettings settingsForInterface(const CAN::InterfaceInfo& itf)
{
  Device::DeviceSettings set;
  set.name = itf.name;
  set.protocol = CANProtocolFactory::static_concreteKey();

  CANSpecificSettings specif;
  specif.interfaceName = itf.name;
  // Not enabled from fdCapable: FD is not a mode switch, but asking for it on a
  // socket whose driver does not really support it is a way to fail at open
  // time for nothing. The database says whether payloads above 8 bytes are
  // expected; the checkbox is one click away when they are.
  specif.fd = false;

  set.deviceSpecificSettings = QVariant::fromValue(specif);
  return set;
}

/**
 * Lists the SocketCAN interfaces of the machine in the device browser.
 *
 * Polled rather than event-driven: netlink would give exact notifications, but
 * it means a socket, a subscription and a reader for a list that changes when
 * somebody plugs a USB adapter in. One sysfs directory listing per second is
 * cheaper than that in every sense.
 */
class CANInterfaceEnumerator final : public Device::DeviceEnumerator
{
public:
  CANInterfaceEnumerator()
  {
    m_timer.setInterval(1000);
    QObject::connect(&m_timer, &QTimer::timeout, this, [this] { rescan(); });
    m_timer.start();
  }

  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
    m_current = CAN::availableInterfaces();
    for(const auto& itf : m_current)
      f(itf.name, settingsForInterface(itf));
  }

private:
  void rescan()
  {
    auto next = CAN::availableInterfaces();
    auto has = [](const std::vector<CAN::InterfaceInfo>& l, const QString& n) {
      return ossia::any_of(l, [&n](const auto& i) { return i.name == n; });
    };

    for(const auto& itf : m_current)
      if(!has(next, itf.name))
        deviceRemoved(itf.name);

    bool added = false;
    for(const auto& itf : next)
    {
      if(!has(m_current, itf.name))
      {
        deviceAdded(itf.name, settingsForInterface(itf));
        added = true;
      }
    }

    m_current = std::move(next);
    if(added)
      sort();
  }

  QTimer m_timer;
  mutable std::vector<CAN::InterfaceInfo> m_current;
};
}

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
  return {{QObject::tr("Interfaces"), new CANInterfaceEnumerator}};
}

const Device::DeviceSettings& CANProtocolFactory::defaultSettings() const noexcept
{
  Device::DeviceSettings s;
  s.protocol = concreteKey();
  s.name = "CAN";

  CANSpecificSettings specif;
  // The first interface actually present, and nothing at all when there is
  // none. Hardcoding "can0" made every fresh CAN device on a machine without a
  // physical CAN port fail at connection time with "no such CAN interface:
  // can0" -- a setting the user never chose and had no reason to suspect.
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
