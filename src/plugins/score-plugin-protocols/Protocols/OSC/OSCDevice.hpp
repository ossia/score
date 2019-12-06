#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{
class OSCDevice final : public Device::OwningDeviceInterface
{
public:
  OSCDevice(const Device::DeviceSettings& stngs);

  bool reconnect() override;
  void recreate(const Device::Node&) final override;

  bool isLearning() const final override;
  void setLearning(bool) final override;
};
}
