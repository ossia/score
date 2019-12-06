#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{

class JoystickDevice final : public Device::OwningDeviceInterface
{
  W_OBJECT(JoystickDevice)
public:
  JoystickDevice(const Device::DeviceSettings& settings);
  ~JoystickDevice();

  bool reconnect() override;
  void disconnect() override;
};
}
