#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>

#include <ossia/detail/config.hpp>

#include <ossia/detail/flat_set.hpp>

#if defined(OSSIA_DNSSD)
#include <QThread>

#include <servus/listener.h>
#include <servus/servus.h>
#endif

namespace Protocols
{
#if defined(OSSIA_DNSSD)
class DNSSDWorker;
class DNSSDEnumerator : public Device::DeviceEnumerator
{
public:
  explicit DNSSDEnumerator(const std::string& service);
  virtual ~DNSSDEnumerator();

  void start();
  void stop();

  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override;

  virtual void addNewDevice(
      const QString& instance, const QString& ip, const QString& port,
      const QMap<QString, QString>& keys) noexcept
      = 0;

protected:
private:
  DNSSDWorker* m_worker{};
  // Avahi only supports being called from *one* thread across an entire execution.
  static QThread* g_dnssd_worker_thread;
};
#endif
}
