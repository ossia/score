#pragma once
#include <Engine/Protocols/OSSIADevice.hpp>

namespace Engine
{
namespace Network
{
class HTTPDevice final : public OwningOSSIADevice
{
public:
  HTTPDevice(const Device::DeviceSettings& settings);

  bool reconnect() override;

};
}
}
