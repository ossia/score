#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{
struct JoystickSpecificSettings;
class JoystickDevice final : public Device::OwningDeviceInterface
{
  W_OBJECT(JoystickDevice)
public:
  JoystickDevice(
      const Device::DeviceSettings& settings,
      const ossia::net::network_context_ptr& ctx);
  ~JoystickDevice();

  bool reconnect() override;
  void disconnect() override;

private:
  template <typename T>
  void do_reconnect(JoystickSpecificSettings& stgs);
  const ossia::net::network_context_ptr& m_ctx;
};
}
