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
  ArtnetDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx);
  ~ArtnetDevice();

  bool reconnect() override;
  void disconnect() override;

private:
  const score::DocumentContext& m_ctx;
};
}
#endif
