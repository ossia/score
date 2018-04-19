#pragma once
#include <Engine/Protocols/OSSIADevice.hpp>

namespace Engine
{
namespace Network
{
class WSDevice final : public OwningOSSIADevice
{
public:
  WSDevice(const Device::DeviceSettings& settings);

  bool reconnect() override;
};
}
}
