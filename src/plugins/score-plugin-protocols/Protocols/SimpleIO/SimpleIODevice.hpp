#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SIMPLEIO)
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{
class SimpleIODevice final : public Device::OwningDeviceInterface
{

  W_OBJECT(SimpleIODevice)
public:
  SimpleIODevice(
      const Device::DeviceSettings& settings,
      const ossia::net::network_context_ptr& ctx);
  ~SimpleIODevice();

  bool reconnect() override;
  void disconnect() override;

private:
  const ossia::net::network_context_ptr& m_ctx;
};
}
#endif
