#pragma once
#include <Engine/Protocols/OSSIADevice.hpp>

namespace Engine
{
namespace Network
{
class OSCQueryDevice final : public OwningOSSIADevice
{
public:
  OSCQueryDevice(const Device::DeviceSettings& settings);

  bool reconnect() override;
  void recreate(const Device::Node& n) override;
};
}
}
