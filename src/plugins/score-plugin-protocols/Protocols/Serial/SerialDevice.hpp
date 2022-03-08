#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SERIAL)
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{
class SerialDevice final : public Device::OwningDeviceInterface
{
public:
  SerialDevice(
      const Device::DeviceSettings& stngs,
      const ossia::net::network_context_ptr& ctx);

  bool reconnect() override;
private:
  const ossia::net::network_context_ptr& m_ctx;
};
}
#endif
