#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{
class MinuitDevice final : public Device::OwningDeviceInterface
{
public:
  MinuitDevice(const Device::DeviceSettings& settings);

  bool reconnect() override;
  void recreate(const Device::Node& n) override;
};
}
