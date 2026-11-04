#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

#include <QTimer>

#include <verdigris>

namespace Protocols
{
class HIDDevice final : public Device::OwningDeviceInterface
{
  W_OBJECT(HIDDevice)
public:
  HIDDevice(
      const Device::DeviceSettings& settings, const ossia::net::network_context_ptr& ctx);
  ~HIDDevice();

  bool reconnect() override;
  void disconnect() override;

  void unplugged() W_SIGNAL(unplugged);

private:
  void on_unplugged();
  const ossia::net::network_context_ptr& m_ctx;
  QTimer m_replug;
};
}
