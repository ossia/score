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
  ArtnetDevice(const Device::DeviceSettings& settings);
  ~ArtnetDevice();

  bool reconnect() override;
  void disconnect() override;
};
}
#endif
