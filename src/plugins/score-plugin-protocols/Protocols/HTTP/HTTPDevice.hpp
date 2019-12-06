#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{
class HTTPDevice final : public Device::OwningDeviceInterface
{
public:
  HTTPDevice(const Device::DeviceSettings& settings);

  bool reconnect() override;
};
}
