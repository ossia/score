// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MQTTProtocolFactory.hpp"

#include "MQTTDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Protocols/LibraryDeviceEnumerator.hpp>
#include <Protocols/MQTT/MQTTProtocolSettingsWidget.hpp>
#include <Protocols/MQTT/MQTTSpecificSettings.hpp>

#include <ossia/network/sockets/configuration.hpp>

#if defined(OSSIA_DNSSD)
#include <Protocols/DNSSDDeviceEnumerator.hpp>
#endif

#include <ossia/network/base/device.hpp>

#include <QObject>
#include <QUrl>

namespace Device
{
class DeviceInterface;
class ProtocolSettingsWidget;
}
struct VisitorVariant;

namespace Protocols
{
QString MQTTProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("MQTT");
}

QString MQTTProtocolFactory::category() const noexcept
{
  return StandardCategories::osc;
}

QUrl MQTTProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/mqtt-device.html");
}
static ossia::net::mqtt5_configuration defaultMQTTConfig() noexcept
{
  ossia::net::mqtt5_configuration config;
  config.transport = ossia::net::tcp_client_configuration{{.host = "127.0.0.1", .port = 1883}};
  return config;
}

ossia::net::mqtt5_configuration readMQTTConfig(const QByteArray& arr)
{
  ossia::net::mqtt5_configuration conf = defaultMQTTConfig();

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
class MQTTTCPEnumerator final : public DNSSDEnumerator
{
public:
  MQTTTCPEnumerator()
      : DNSSDEnumerator{"_mqtt._tcp"}
  {
    start();
  }
  ~MQTTTCPEnumerator() { stop(); }

private:
  void addNewDevice(
      const QString& instance, const QString& ip, const QString& port,
      const QMap<QString, QString>& keys) noexcept override
  {
    using namespace std::literals;

    Device::DeviceSettings set;
    set.name = instance;
    set.protocol = MQTTProtocolFactory::static_concreteKey();

    MQTTSpecificSettings sub;
    ossia::net::tcp_client_configuration conf;
    conf.host = ip.toStdString();
    conf.port = port.toInt();
    sub.configuration.transport = conf;

    set.deviceSpecificSettings = QVariant::fromValue(std::move(sub));
    deviceAdded(set.name, set);
  }
};

#endif

Device::DeviceEnumerators
MQTTProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{

  return {
#if defined(OSSIA_DNSSD)
      {"TCP", new MQTTTCPEnumerator{}}
#endif
  };
}

Device::DeviceInterface* MQTTProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new MQTTDevice{settings, plugin.networkContext()};
}

const Device::DeviceSettings& MQTTProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "MQTT";
    MQTTSpecificSettings specif;
    specif.configuration.transport = ossia::net::tcp_client_configuration{"127.0.0.1", 1883};
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* MQTTProtocolFactory::makeSettingsWidget()
{
  return new MQTTProtocolSettingsWidget;
}

QVariant
MQTTProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<MQTTSpecificSettings>(visitor);
}

void MQTTProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<MQTTSpecificSettings>(data, visitor);
}

bool MQTTProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  return true;
}
}
