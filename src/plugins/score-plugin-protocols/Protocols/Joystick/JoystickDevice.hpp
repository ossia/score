#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{

class JoystickDevice final : public Device::OwningDeviceInterface
{
  W_OBJECT(JoystickDevice)
public:
  JoystickDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx);
  ~JoystickDevice();

  bool reconnect() override;
  void disconnect() override;

private:
  const score::DocumentContext& m_ctx;
};
}
