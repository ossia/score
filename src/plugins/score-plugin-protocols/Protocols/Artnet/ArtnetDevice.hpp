#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{
class ArtnetDevice final : public Device::OwningDeviceInterface
{

  W_OBJECT(ArtnetDevice)
public:
  ArtnetDevice(
      const Device::DeviceSettings& settings,
      const ossia::net::network_context_ptr& ctx);
  ~ArtnetDevice();

  bool reconnect() override;
  void disconnect() override;

private:
  const ossia::net::network_context_ptr& m_ctx;
};
}
#endif
