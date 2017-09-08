#pragma once
#include <Engine/Protocols/OSSIADevice.hpp>

namespace score
{
struct DeviceSettings;
} // namespace score

namespace Engine
{
namespace Network
{
class MIDIDevice final : public OwningOSSIADevice
{
public:
  MIDIDevice(const Device::DeviceSettings& settings);

  bool reconnect() override;

  void disconnect() override;

  using OwningOSSIADevice::refresh;
  Device::Node refresh() override;
};
}
}
