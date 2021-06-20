#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{

class WiimoteDevice final : public Device::OwningDeviceInterface
{

  W_OBJECT(WiimoteDevice)
public:
  WiimoteDevice(
      const Device::DeviceSettings& settings,
      const ossia::net::network_context_ptr& ctx);
  ~WiimoteDevice();

  bool reconnect() override;
  void disconnect() override;

private:
  const ossia::net::network_context_ptr& m_ctx;
};

}
