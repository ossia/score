#pragma once
#include <Engine/Protocols/OSSIADevice.hpp>

namespace Engine
{
namespace Network
{
class SerialDevice final : public OwningOSSIADevice
{
public:
  SerialDevice(const Device::DeviceSettings& settings);

  bool reconnect() override;

};
}
}
