#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{

class LeapmotionDevice final : public Device::OwningDeviceInterface
{

  W_OBJECT(LeapmotionDevice)
public:
  LeapmotionDevice(
      const Device::DeviceSettings& settings,
      const ossia::net::network_context_ptr& ctx);
  ~LeapmotionDevice();

  bool reconnect() override;
  void disconnect() override;

private:
  const ossia::net::network_context_ptr& m_ctx;
};

}
