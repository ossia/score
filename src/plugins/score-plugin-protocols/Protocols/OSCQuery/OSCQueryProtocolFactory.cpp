// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OSCQueryProtocolFactory.hpp"

#include "OSCQueryDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>
#include <Protocols/OSCQuery/OSCQueryProtocolSettingsWidget.hpp>
#include <Protocols/OSCQuery/OSCQuerySpecificSettings.hpp>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#if defined(OSSIA_DNSSD)
#include <servus/servus.h>
#include <servus/listener.h>
#include <asio/io_service.hpp>
#include <asio/ip/basic_resolver.hpp>
#include <asio/ip/tcp.hpp>
#endif
#include <ossia/network/base/device.hpp>

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>

namespace Protocols
{
#if defined(OSSIA_DNSSD)
class OSCQueryEnumerator final
    : public Device::DeviceEnumerator
    , public servus::Listener
{
public:
  OSCQueryEnumerator():
    m_serv{"_oscjson._tcp"}
  {
    m_serv.addListener(static_cast<servus::Listener*>(this));
    m_serv.beginBrowsing(servus::Interface::IF_ALL);
    m_serv.browse(100);
    m_timer = startTimer(250);

  }
  ~OSCQueryEnumerator()
  {
    killTimer(m_timer);
    m_serv.removeListener(this);
    m_serv.endBrowsing();
  }

private:
  mutable QNetworkAccessManager m_http;

  // Servus API
  void instanceAdded(const std::string& instance) override
  {
    if(m_instances.count(instance) == 0)
    {
      m_instances.insert(instance);
      deviceAdded(settingsForInstance(instance));
    }
  }

  void instanceRemoved(const std::string& instance) override
  {
    if(m_instances.count(instance) != 0)
    {
      m_instances.erase(instance);
      deviceRemoved(QString::fromStdString(instance));
    }
  }

  Device::DeviceSettings settingsForInstance(const std::string& instance) const noexcept
  {
    using namespace std::literals;

    Device::DeviceSettings set;
    set.name = QString::fromStdString(instance);
    set.protocol = OSCQueryProtocolFactory::static_concreteKey();
    OSCQuerySpecificSettings sub;

    std::string ip = m_serv.get(instance, "servus_host");
    if(ip.empty())
    {
      ip = m_serv.get(instance, "servus_ip");
    }

#if defined(_WIN32)
    if(!ip.empty() && ip.back() == '.')
      ip.pop_back();
#endif

    std::string port = m_serv.get(instance, "servus_port");
    bool websockets = m_serv.get(instance, "WebSockets") == "true"sv;

    try
    {
      asio::io_service io_service;

      asio::ip::tcp::resolver resolver(io_service);
      asio::ip::tcp::resolver::iterator iter = resolver.resolve(asio::ip::tcp::v4(), ip, port,
                                                                asio::ip::resolver_base::numeric_service
                                                                );

      asio::ip::tcp::resolver::iterator end;
      while(iter != end)
      {
        if (iter->endpoint().address().is_loopback())
        {
          ip = "localhost";
          break;
        }
        else
        {
          ip = iter->endpoint().address().to_string();
          ++iter;
        }
      }
    }
    catch(...) {
    }

    {
      QEventLoop e;

      QString req = QString("http://%1:%2/?HOST_INFO")
          .arg(QString::fromStdString(ip))
          .arg(QString::fromStdString(port));


      QNetworkRequest qreq{QUrl(req)};
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
      qreq.setTransferTimeout(1000);
#endif
      QPointer<QNetworkReply> ret = m_http.get(qreq);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
      connect(ret, &QNetworkReply::errorOccurred,
              this, [&e] { e.exit(); });
#else
      connect(ret, qOverload<QNetworkReply::NetworkError>(&QNetworkReply::error),
              this, [&e] { e.exit(); });
#endif
#if QT_CONFIG(ssl)
      connect(ret, &QNetworkReply::sslErrors,
              this, [&e] { e.exit(); });
#endif
      connect(ret, &QNetworkReply::finished,
              this, [=,&set, &e] {
        auto doc = QJsonDocument::fromJson(ret->readAll());
        QString newName = doc.object()["NAME"].toString();
        if(!newName.isEmpty())
          set.name = newName;

        ret->deleteLater();
        e.exit();
      });

      e.exec();
      if(ret)
      {
        ret->deleteLater();
      }
    }

    sub.host = QString("%1://%2:%3")
        .arg(websockets ? "ws" : "http")
        .arg(ip.c_str())
        .arg(port.c_str())
    ;

    set.deviceSpecificSettings = QVariant::fromValue(std::move(sub));
    return set;
  }

  // DeviceEnumerator API
  void enumerate(std::function<void(const Device::DeviceSettings&)> f) const override
  {
    for(const auto& instance : m_instances)
    {
      f(settingsForInstance(instance));
    }
  }

  void timerEvent(QTimerEvent* ev) override
  {
    m_serv.browse(0);
  }

  servus::Servus m_serv;
  ossia::flat_set<std::string> m_instances;
  int m_timer{-1};
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

int OSCQueryProtocolFactory::visualPriority() const noexcept
{
  return 2;
}

Device::DeviceEnumerator* OSCQueryProtocolFactory::getEnumerator(const score::DocumentContext& ctx) const
{
#if defined(OSSIA_DNSSD)
  return new OSCQueryEnumerator;
#else
  return nullptr;
#endif
}

Device::DeviceInterface* OSCQueryProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const score::DocumentContext& ctx)
{
  return new OSCQueryDevice{settings};
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
    const Device::DeviceSettings& b) const noexcept
{
  return a.name != b.name;
}
}
