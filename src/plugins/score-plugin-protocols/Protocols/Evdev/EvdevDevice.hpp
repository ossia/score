#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_EVDEV)
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{
class EvdevDevice final : public Device::OwningDeviceInterface
{

  W_OBJECT(EvdevDevice)
public:
  EvdevDevice(
      const Device::DeviceSettings& settings,
      const ossia::net::network_context_ptr& ctx);
  ~EvdevDevice();

  bool reconnect() override;
  void disconnect() override;

private:
  const ossia::net::network_context_ptr& m_ctx;
};
}
#endif
