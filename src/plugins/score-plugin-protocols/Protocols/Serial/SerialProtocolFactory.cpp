// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SERIAL)
#include "SerialDevice.hpp"
#include "SerialProtocolFactory.hpp"

#include <Device/Protocol/DeviceSettings.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Protocols/LibraryDeviceEnumerator.hpp>
#include <Protocols/Serial/SerialProtocolSettingsWidget.hpp>
#include <Protocols/Serial/SerialSpecificSettings.hpp>

#include <ossia/network/base/device.hpp>

#include <QObject>
#include <QUrl>

#if defined(__EMSCRIPTEN__)
#include <Protocols/Serial/WebSerial.hpp>

#include <QTimer>

#include <algorithm>
#endif

namespace Protocols
{
#if defined(__EMSCRIPTEN__)
namespace
{
Device::DeviceSettings toDeviceSettings(const serial::port_info& port)
{
  SerialSpecificSettings specif;
  specif.port = port;
  specif.rate = 9600;

  Device::DeviceSettings s;
  s.name = QString::fromStdString(port.port_name);
  s.protocol = SerialProtocolFactory::static_concreteKey();
  s.deviceSpecificSettings = QVariant::fromValue(specif);
  return s;
}

class WebSerialEnumerator final : public Device::DeviceEnumerator
{
public:
  WebSerialEnumerator()
  {
    m_timer.setInterval(250);
    connect(&m_timer, &QTimer::timeout, this, &WebSerialEnumerator::poll);
  }

  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
    for(const auto& p : m_known)
      f(QString::fromStdString(p.port_name), toDeviceSettings(p));

    // getPorts() only reports ports the user already granted access to, and it
    // is asynchronous: the list has to be rescanned rather than read once.
    WebSerial::scan();
    m_timer.start();
  }

private:
  void poll()
  {
    const int gen = WebSerial::generation();
    if(gen == m_generation)
      return;
    m_generation = gen;

    auto found = serial::available_ports();

    for(const auto& p : m_known)
      if(std::none_of(found.begin(), found.end(), [&](const serial::port_info& o) {
           return o.system_location == p.system_location;
         }))
        deviceRemoved(QString::fromStdString(p.port_name));

    for(const auto& p : found)
      if(std::none_of(m_known.begin(), m_known.end(), [&](const serial::port_info& o) {
           return o.system_location == p.system_location;
         }))
        deviceAdded(QString::fromStdString(p.port_name), toDeviceSettings(p));

    m_known = std::move(found);
    sort();
  }

  mutable QTimer m_timer;
  std::vector<serial::port_info> m_known;
  int m_generation{-1};
};
}
#endif

QString SerialProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Serial");
}

QString SerialProtocolFactory::category() const noexcept
{
  return StandardCategories::hardware;
}

QUrl SerialProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/serial-device.html");
}
Device::DeviceEnumerators
SerialProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  auto library_enumerator = new LibraryDeviceEnumerator{
      "Ossia.Serial",
      {"qml"},
      SerialProtocolFactory::static_concreteKey(),
      [](const QByteArray& arr) {
    return QVariant::fromValue(SerialSpecificSettings{{}, arr});
      },
      ctx};

#if defined(__EMSCRIPTEN__)
  if(WebSerial::available())
    return {{"Serial ports", new WebSerialEnumerator}, {"Library", library_enumerator}};
#endif

  return {{"Library", library_enumerator}};
}

Device::DeviceInterface* SerialProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new SerialDevice{settings, plugin.networkContext()};
}

const Device::DeviceSettings& SerialProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Serial";
    SerialSpecificSettings specif;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* SerialProtocolFactory::makeSettingsWidget()
{
  return new SerialProtocolSettingsWidget;
}

QVariant
SerialProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<SerialSpecificSettings>(visitor);
}

void SerialProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<SerialSpecificSettings>(data, visitor);
}

bool SerialProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  return true;
}
}
#endif
