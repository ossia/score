// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OSCProtocolFactory.hpp"

#include "OSCDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

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

static ossia::net::osc_protocol_configuration defaultOSCConfig() noexcept
{
  ossia::net::osc_protocol_configuration config;
  config.mode = ossia::net::osc_protocol_configuration::MIRROR;
  config.version = ossia::net::osc_protocol_configuration::OSC1_0;
  ossia::net::udp_configuration udp;
  udp.local = ossia::net::receive_socket_configuration{"0.0.0.0", 9996};
  udp.remote = ossia::net::send_socket_configuration{"127.0.0.1", 9997};
  config.transport = udp;
  return config;
}

ossia::net::osc_protocol_configuration readOSCConfig(const QByteArray& arr)
{
  ossia::net::osc_protocol_configuration conf = defaultOSCConfig();

  rapidjson::Document doc;
  doc.Parse(arr.data(), arr.length());
  if(!doc.IsObject())
    return {};

  if(auto dev_it = doc.FindMember("Device"); dev_it != doc.MemberEnd())
  {
    auto dev = dev_it->value.GetObject();
    if(dev.HasMember("Config"))
      conf <<= JSONWriter{dev}.obj["Config"];
  }

  return conf;
}

struct OSCCompatibleCheck
{
  const ossia::net::osc_protocol_configuration &config1, config2;
  bool operator()(
      const ossia::net::udp_configuration& lhs,
      const ossia::net::udp_configuration& rhs) const noexcept
  {
    if(lhs.local && rhs.local && lhs.local->port == rhs.local->port)
      return false;
    return true;
  }
  bool operator()(
      const ossia::net::unix_dgram_configuration& lhs,
      const ossia::net::unix_dgram_configuration& rhs) const noexcept
  {
    if(lhs.local && rhs.local && lhs.local->fd == rhs.local->fd)
      return false;
    return true;
  }
  bool operator()(
      const ossia::net::unix_stream_configuration& lhs,
      const ossia::net::unix_dgram_configuration& rhs) const noexcept
  {
    if(rhs.local && lhs.fd == rhs.local->fd)
      return false;
    return true;
  }
  bool operator()(
      const ossia::net::unix_dgram_configuration& lhs,
      const ossia::net::unix_stream_configuration& rhs) const noexcept
  {
    if(lhs.local && rhs.fd == lhs.local->fd)
      return false;
    return true;
  }
  bool operator()(
      const ossia::net::tcp_configuration& lhs,
      const ossia::net::tcp_configuration& rhs) const noexcept
  {
    if(config1.mode == ossia::net::osc_protocol_configuration::HOST
       && config2.mode == ossia::net::osc_protocol_configuration::HOST
       && lhs.port == rhs.port)
      return false;
    return true;
  }
  bool operator()(
      const ossia::net::ws_server_configuration& lhs,
      const ossia::net::ws_server_configuration& rhs) const noexcept
  {
    if(config1.mode == ossia::net::osc_protocol_configuration::HOST
       && config2.mode == ossia::net::osc_protocol_configuration::HOST
       && lhs.port == rhs.port)
      return false;
    return true;
  }
  bool operator()(
      const ossia::net::tcp_configuration& lhs,
      const ossia::net::ws_server_configuration& rhs) const noexcept
  {
    if(config1.mode == ossia::net::osc_protocol_configuration::HOST
       && config2.mode == ossia::net::osc_protocol_configuration::HOST
       && lhs.port == rhs.port)
      return false;
    return true;
  }
  bool operator()(
      const ossia::net::ws_server_configuration& lhs,
      const ossia::net::tcp_configuration& rhs) const noexcept
  {
    if(config1.mode == ossia::net::osc_protocol_configuration::HOST
       && config2.mode == ossia::net::osc_protocol_configuration::HOST
       && lhs.port == rhs.port)
      return false;
    return true;
  }
  bool operator()(
      const ossia::net::unix_stream_configuration& lhs,
      const ossia::net::unix_stream_configuration& rhs) const noexcept
  {
    if(config1.mode == ossia::net::osc_protocol_configuration::HOST
       && config2.mode == ossia::net::osc_protocol_configuration::HOST
       && lhs.fd == rhs.fd)
      return false;
    return true;
  }
  template <typename LHS, typename RHS>
  bool operator()(const LHS&, const RHS&) const noexcept
  {
    return true;
  }
};

Device::DeviceEnumerators
OSCProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  auto library_enumerator = new LibraryDeviceEnumerator{
      "9a42de4b-f6eb-4bca-9564-01b975f601b9",
      {"json", "device", "touchosc", "xml"},
      OSCProtocolFactory::static_concreteKey(),
      [](const QByteArray& arr) {
    auto copy = arr;
    copy.detach();

    return QVariant::fromValue(
        OSCSpecificSettings{readOSCConfig(copy), std::nullopt, std::move(copy)});
      },
      ctx};

  return {{"Library", library_enumerator}};
}

Device::DeviceInterface* OSCProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new OSCDevice{settings, plugin.networkContext()};
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

QVariant
OSCProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<OSCSpecificSettings>(visitor);
}

void OSCProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<OSCSpecificSettings>(data, visitor);
}

bool OSCProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  auto a_p = a.deviceSpecificSettings.value<OSCSpecificSettings>();
  auto b_p = b.deviceSpecificSettings.value<OSCSpecificSettings>();
  return a.name != b.name
         && visit(
             OSCCompatibleCheck{a_p.configuration, b_p.configuration},
             a_p.configuration.transport, b_p.configuration.transport);
}
}
