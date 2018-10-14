#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

#include <ossia/network/base/device.hpp>

#include <wobjectdefs.h>


namespace Protocols
{
class OSCQueryDevice final : public Device::OwningDeviceInterface
{
  W_OBJECT(OSCQueryDevice)
public:
  OSCQueryDevice(const Device::DeviceSettings& settings);

  bool reconnect() override;
  void disconnect() override;
  void recreate(const Device::Node& n) override;

public:
  void sig_command() W_SIGNAL(sig_command);
  void sig_disconnect() W_SIGNAL(sig_disconnect);

private:
  void slot_command();
  W_SLOT(slot_command);
};
}
