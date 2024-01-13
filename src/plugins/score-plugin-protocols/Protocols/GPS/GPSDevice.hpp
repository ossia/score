#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_GPS)
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{
class GPSDevice final : public Device::OwningDeviceInterface
{

  W_OBJECT(GPSDevice)
public:
  GPSDevice(
      const Device::DeviceSettings& settings,
      const ossia::net::network_context_ptr& ctx);
  ~GPSDevice();

  bool reconnect() override;
  void disconnect() override;

private:
  const ossia::net::network_context_ptr& m_ctx;
};
}
#endif
