// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CoAPProtocolFactory.hpp"

#include "CoAPDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Protocols/LibraryDeviceEnumerator.hpp>
#include <Protocols/CoAP/CoAPProtocolSettingsWidget.hpp>
#include <Protocols/CoAP/CoAPSpecificSettings.hpp>

#include <ossia/network/sockets/configuration.hpp>

#if defined(OSSIA_DNSSD)
#include <Protocols/DNSSDDeviceEnumerator.hpp>
#endif

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
QString CoAPProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("CoAP");
}

QString CoAPProtocolFactory::category() const noexcept
{
  return StandardCategories::osc;
}

static ossia::net::coap_client_configuration defaultCoAPConfig() noexcept
{
  ossia::net::coap_client_configuration config;
  config.transport
      = ossia::net::udp_configuration{}; //{.host = "127.0.0.1", .port = 5683}, {}};
  return config;
}

ossia::net::coap_client_configuration readCoAPConfig(const QByteArray& arr)
{
  ossia::net::coap_client_configuration conf = defaultCoAPConfig();

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

#if defined(OSSIA_DNSSD)
class CoAPTCPEnumerator final : public DNSSDEnumerator
{
public:
  CoAPTCPEnumerator()
      : DNSSDEnumerator{"_coap._tcp"}
  {
    start();
  }
  ~CoAPTCPEnumerator() { stop(); }

private:
  void addNewDevice(
      const QString& instance, const QString& ip, const QString& port,
      const QMap<QString, QString>& keys) noexcept override
  {
    using namespace std::literals;

    Device::DeviceSettings set;
    set.name = instance;
    set.protocol = CoAPProtocolFactory::static_concreteKey();

    CoAPSpecificSettings sub;
    ossia::net::tcp_configuration conf;
    conf.host = ip.toStdString();
    conf.port = port.toInt();
    sub.configuration.transport = conf;

    set.deviceSpecificSettings = QVariant::fromValue(std::move(sub));
    deviceAdded(set.name, set);
  }
};

#endif

Device::DeviceEnumerators
CoAPProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  return {
#if defined(OSSIA_DNSSD)
      {"TCP", new CoAPTCPEnumerator{}}
#endif
  };
}

Device::DeviceInterface* CoAPProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new CoAPDevice{settings, plugin.networkContext()};
}

const Device::DeviceSettings& CoAPProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "CoAP";
    CoAPSpecificSettings specif;
    specif.configuration.transport = ossia::net::udp_configuration{
        {.local = {},
         .remote = ossia::net::send_socket_configuration{"127.0.0.1", 5683}}};
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* CoAPProtocolFactory::makeSettingsWidget()
{
  return new CoAPProtocolSettingsWidget;
}

QVariant
CoAPProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<CoAPSpecificSettings>(visitor);
}

void CoAPProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<CoAPSpecificSettings>(data, visitor);
}

bool CoAPProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  return true;
}
}
