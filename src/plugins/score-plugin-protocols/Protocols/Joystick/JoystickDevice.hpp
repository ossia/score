#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{

class JoystickDevice final : public Device::OwningDeviceInterface
{
  W_OBJECT(JoystickDevice)
public:
  JoystickDevice(
      const Device::DeviceSettings& settings,
      const ossia::net::network_context_ptr& ctx);
  ~JoystickDevice();

  bool reconnect() override;
  void disconnect() override;

private:
  const ossia::net::network_context_ptr& m_ctx;
};
}
