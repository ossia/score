#include "DNSSDDeviceEnumerator.hpp"

#if defined(OSSIA_DNSSD)
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <QTimer>

#include <wobjectimpl.h>

namespace Protocols
{
class DNSSDWorker
    : public QObject
    , public servus::Listener

{
  W_OBJECT(DNSSDWorker)
public:
  explicit DNSSDWorker(const std::string& service)
      : m_serv{service}
  {
    m_serv.addListener(static_cast<servus::Listener*>(this));
  }

  void instanceAdded(const std::string& instance) override
  {
    // Already registered:
    if(m_instances.count(instance) != 0)
      return;

    m_instances.insert(instance);

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

    try
    {
      boost::asio::io_service io_service;

      boost::asio::ip::tcp::resolver resolver(io_service);
      boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(
          boost::asio::ip::tcp::v4(), ip, port,
          boost::asio::ip::resolver_base::numeric_service);

      boost::asio::ip::tcp::resolver::iterator end;
      while(iter != end)
      {
        if(iter->endpoint().address().is_loopback())
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
    catch(const std::exception& e)
    {
      qDebug() << "could not resolve host:" << ip.c_str() << " => " << e.what();
    }

    QMap<QString, QString> keys;

    for(auto& k : m_serv.getKeys(instance))
      keys[QString::fromStdString(k)] = QString::fromStdString(m_serv.get(instance, k));

    on_newHost(
        QString::fromStdString(instance), QString::fromStdString(ip),
        QString::fromStdString(port), keys);
  }

  void instanceRemoved(const std::string& instance) override
  {
    if(m_instances.count(instance) != 0)
    {
      m_instances.erase(instance);
      on_removedHost(QString::fromStdString(instance));
    }
  }

  void start()
  {
    m_serv.beginBrowsing(servus::Interface::IF_ALL);
    m_serv.browse(100);
    m_timer = startTimer(250);
  }

  void stop()
  {
    killTimer(m_timer);
    m_serv.removeListener(this);
    m_serv.endBrowsing();
  }

  void timerEvent(QTimerEvent* ev) override { m_serv.browse(10); }

  void on_newHost(
      const QString& name, const QString& ip, const QString& port,
      const QMap<QString, QString>& keys) W_SIGNAL(on_newHost, name, ip, port, keys);
  void on_removedHost(QString name) W_SIGNAL(on_removedHost, name);

  servus::Servus m_serv;
  ossia::flat_set<std::string> m_instances;
  int m_timer{-1};
};
}

W_REGISTER_ARGTYPE(QMap<QString, QString>)
W_OBJECT_IMPL(Protocols::DNSSDWorker)

namespace Protocols
{
DNSSDEnumerator::DNSSDEnumerator(const std::string& service)
{
  m_worker = new DNSSDWorker{service};
}

DNSSDEnumerator::~DNSSDEnumerator() { }

void DNSSDEnumerator::start()
{
  m_worker->moveToThread(&m_workerThread);
  connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
  connect(m_worker, &DNSSDWorker::on_newHost, this, &DNSSDEnumerator::addNewDevice);
  connect(m_worker, &DNSSDWorker::on_removedHost, this, &DNSSDEnumerator::deviceRemoved);
  m_workerThread.start();

  QMetaObject::invokeMethod(m_worker, &DNSSDWorker::start, Qt::QueuedConnection);
}

void DNSSDEnumerator::stop()
{
  QMetaObject::invokeMethod(m_worker, &DNSSDWorker::stop, Qt::QueuedConnection);
  m_workerThread.quit();
  m_workerThread.wait();
}

void DNSSDEnumerator::enumerate(
    std::function<void(const QString&, const Device::DeviceSettings&)> f) const
{
}

#endif
}
