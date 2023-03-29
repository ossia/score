#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>

#include <ossia/detail/config.hpp>

#include <ossia/detail/flat_set.hpp>

#if defined(OSSIA_DNSSD)
#include <servus/listener.h>
#include <servus/servus.h>
#endif

namespace Protocols
{
#if defined(OSSIA_DNSSD)
class DNSSDEnumerator
    : public Device::DeviceEnumerator
    , public servus::Listener
{
public:
  explicit DNSSDEnumerator(const std::string& service);
  virtual ~DNSSDEnumerator();

  void start();

  void enumerate(std::function<void(const Device::DeviceSettings&)> f) const override;
  void timerEvent(QTimerEvent* ev) override;

  void instanceAdded(const std::string& instance) override;
  void instanceRemoved(const std::string& instance) override;

  virtual void addNewDevice(
      const std::string& instance, const std::string& ip,
      const std::string& port) noexcept
      = 0;

protected:
  servus::Servus m_serv;
  ossia::flat_set<std::string> m_instances;

private:
  int m_timer{-1};
};
#endif
}
