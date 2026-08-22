#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_CAN)
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{
class CANDevice final : public Device::OwningDeviceInterface
{
  W_OBJECT(CANDevice)
public:
  CANDevice(
      const Device::DeviceSettings& settings,
      const ossia::net::network_context_ptr& ctx);
  ~CANDevice();

  bool reconnect() override;
  void disconnect() override;

private:
  const ossia::net::network_context_ptr& m_ctx;
};
}
#endif
