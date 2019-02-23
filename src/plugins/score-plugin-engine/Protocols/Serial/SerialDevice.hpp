#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SERIAL)
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{
class SerialDevice final : public Device::OwningDeviceInterface
{
public:
  SerialDevice(const Device::DeviceSettings& settings);

  bool reconnect() override;
};
}
#endif
