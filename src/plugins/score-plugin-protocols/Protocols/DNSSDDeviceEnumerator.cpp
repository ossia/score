#include "DNSSDDeviceEnumerator.hpp"

#if defined(OSSIA_DNSSD)

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace Protocols
{

DNSSDEnumerator::DNSSDEnumerator(const std::string& service)
    : m_serv{service}
{
  m_serv.addListener(static_cast<servus::Listener*>(this));
  m_timer = startTimer(250);
}

DNSSDEnumerator::~DNSSDEnumerator()
{
  killTimer(m_timer);
  m_serv.removeListener(this);
  m_serv.endBrowsing();
}

void DNSSDEnumerator::start()
{
  // Note: must be called in the constructor of child classes, never in the parent
  // class as the addNewDevice virtual method is still pure virtual
  QTimer::singleShot(1, this, [this] {
    m_serv.beginBrowsing(servus::Interface::IF_ALL);
    m_serv.browse(100);
  });
}

void DNSSDEnumerator::enumerate(
    std::function<void(const Device::DeviceSettings&)> f) const
{
}

void DNSSDEnumerator::timerEvent(QTimerEvent* ev)
{
  m_serv.browse(0);
}

void DNSSDEnumerator::instanceAdded(const std::string& instance)
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

  addNewDevice(instance, ip, port);
}

void DNSSDEnumerator::instanceRemoved(const std::string& instance)
{
  if(m_instances.count(instance) != 0)
  {
    m_instances.erase(instance);
    deviceRemoved(QString::fromStdString(instance));
  }
}
#endif
}
