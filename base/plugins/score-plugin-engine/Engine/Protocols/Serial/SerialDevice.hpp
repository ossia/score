#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

namespace Engine
{
namespace Network
{
class SerialDevice final : public Device::OwningDeviceInterface
{
public:
  SerialDevice(const Device::DeviceSettings& settings);

  bool reconnect() override;
};
}
}
