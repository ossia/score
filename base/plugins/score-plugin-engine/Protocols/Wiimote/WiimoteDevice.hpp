#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

namespace Engine::Network {


class WiimoteDevice final : public Device::OwningDeviceInterface
{

  W_OBJECT(WiimoteDevice)
public:
  WiimoteDevice(const Device::DeviceSettings& settings);
  ~WiimoteDevice();

  bool reconnect() override;
  void disconnect() override;
};

}
