#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

namespace Engine
{
namespace Network
{
class WSDevice final : public Device::OwningDeviceInterface
{
public:
  WSDevice(const Device::DeviceSettings& settings);

  bool reconnect() override;
};
}
}
