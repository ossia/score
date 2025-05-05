// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OSCQueryProtocolFactory.hpp"

#include "OSCQueryDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Protocols/OSCQuery/OSCQueryProtocolSettingsWidget.hpp>
#include <Protocols/OSCQuery/OSCQuerySpecificSettings.hpp>

#include <score/tools/ListNetworkAddresses.hpp>

#include <QNetworkAccessManager>
#include <QNetworkReply>

#if defined(OSSIA_DNSSD)
#include <Protocols/DNSSDDeviceEnumerator.hpp>
#endif

#include <ossia/network/base/device.hpp>

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QUrl>

namespace Protocols
{
#if defined(OSSIA_DNSSD)
class OSCQueryEnumerator final : public DNSSDEnumerator
{
public:
  QStringList local_ips;
  OSCQueryEnumerator()
      : DNSSDEnumerator{"_oscjson._tcp"}
  {
    local_ips = score::list_ipv4_for_connecting();
    start();
  }
  ~OSCQueryEnumerator() { stop(); }

private:
  void addNewDevice(
      const QString& instance, const QString& ip, const QString& port,
      const QMap<QString, QString>& keys) noexcept override
  {
    const bool websockets = keys.value("WebSockets", "false").toLower() == "true";
    if(local_ips.size() > 1 && local_ips.contains(ip))
    {
      do_addNewDevice_localip(instance, websockets, port);
    }
    else
    {
      do_addNewDevice_other(instance, websockets, ip, port);
    }
  }

  void
  do_addNewDevice_localip(const QString& instance, bool websockets, const QString& port)
  {
    auto done = std::make_shared<bool>(false);

    // We do this as some software inadvertently bind on only one local IP...
    for(const auto& ip : local_ips)
    {
      // Launch non-=1 version as some hosts don't like that
      make_request(
          QUrl(QString("http://%1:%2/?HOST_INFO").arg(ip).arg(port)), instance,
          websockets, ip, port, done);

      // This one's OSCQuery server crashes if being sent a =1
      if(instance.startsWith("Chromatik"))
        continue;

      // Launch =1 version as some webservers don't like non k=v queries
      make_request(
          QUrl(QString("http://%1:%2/?HOST_INFO=1").arg(ip).arg(port)), instance,
          websockets, ip, port, done);
    }
  }

  void do_addNewDevice_other(
      const QString& instance, bool websockets, const QString& ip, const QString& port)
  {
    auto done = std::make_shared<bool>(false);
    using namespace std::literals;
    make_request(
        QUrl(QString("http://%1:%2/?HOST_INFO").arg(ip).arg(port)), instance, websockets,
        ip, port, done);

    // This one's OSCQuery server crashes if being sent a =1
    if(instance.startsWith("Chromatik"))
      return;

    make_request(
        QUrl(QString("http://%1:%2/?HOST_INFO=1").arg(ip).arg(port)), instance,
        websockets, ip, port, done);
  }

  void make_request(
      QUrl req, const QString& instance, bool websockets, const QString& ip,
      const QString& port, const std::shared_ptr<bool>& done)
  {
    QNetworkRequest qreq{req};
    qreq.setTransferTimeout(1000);
    QPointer<QNetworkReply> ret = m_http.get(qreq);

    connect(
        ret, &QNetworkReply::errorOccurred, this,
        [req, ret](QNetworkReply::NetworkError err) {
      ret->deleteLater();
      qDebug() << req << err;
    });

#if QT_CONFIG(ssl)
    connect(
        ret, &QNetworkReply::sslErrors, this,
        [req, ret](const QList<QSslError>& errors) {
      ret->deleteLater();
      qDebug() << req << errors;
    });
#endif

    connect(
        ret, &QNetworkReply::finished, this,
        [this, req, ret, instance, websockets, ip, port, done]() mutable {
      do_request(ret, instance, websockets, ip, port, *done);
      ret->deleteLater();
    });
  }

  void do_request(
      QPointer<QNetworkReply> ret, const QString& instance, bool websockets,
      const QString& ip, const QString& port, bool& done)
  {
    if(done)
      return;

    auto doc = QJsonDocument::fromJson(ret->readAll());

    if(!doc.isObject())
      return;

    auto obj = doc.object();
    if(obj.contains("FULL_PATH"))
      return;

    done = true;

    on_finished(obj, instance, websockets, ip, port);
  }

  void on_finished(
      QJsonObject obj, const QString& instance, bool websockets, const QString& ip,
      const QString& port)
  {
    Device::DeviceSettings set;
    set.name = instance;
    set.protocol = OSCQueryProtocolFactory::static_concreteKey();

    QString newName = obj["NAME"].toString();

    if(!newName.isEmpty())
      set.name = newName;
    QString ws_ip = obj["WS_IP"].toString();
    QString ws_port = obj["WS_PORT"].toString();
    websockets |= (!ws_ip.isEmpty() || !ws_port.isEmpty());
    OSCQuerySpecificSettings sub;
    sub.host = QString("%1://%2:%3")
                   .arg(websockets ? "ws" : "http")
                   .arg(ws_ip.isEmpty() ? ip : ws_ip)
                   .arg(ws_port.isEmpty() ? port : ws_port);
    sub.dense = obj["EXTENSIONS"].toObject()["DENSE"].toBool();
    set.deviceSpecificSettings = QVariant::fromValue(std::move(sub));
    deviceAdded(set.name, set);
  }

  mutable QNetworkAccessManager m_http;
};

#endif

QString OSCQueryProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("OSCQuery");
}

QString OSCQueryProtocolFactory::category() const noexcept
{
  return StandardCategories::osc;
}

QUrl OSCQueryProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/oscquery-device.html");
}

int OSCQueryProtocolFactory::visualPriority() const noexcept
{
  return 2;
}

Device::DeviceEnumerators
OSCQueryProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
#if defined(OSSIA_DNSSD)
  try
  {
    return {{"Network", new OSCQueryEnumerator}};
  }
  catch(...)
  {
  }
#endif
  return {};
}

Device::DeviceInterface* OSCQueryProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new OSCQueryDevice{settings, plugin.networkContext()};
}

const Device::DeviceSettings& OSCQueryProtocolFactory::defaultSettings() const noexcept
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

QVariant OSCQueryProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<OSCQuerySpecificSettings>(visitor);
}

void OSCQueryProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<OSCQuerySpecificSettings>(data, visitor);
}

bool OSCQueryProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  return true;
}
}
