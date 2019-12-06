#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

#include <ossia/network/base/device.hpp>

#include <verdigris>

namespace ossia::oscquery
{
class oscquery_mirror_protocol;
}
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
  bool connected() const override;

public:
  void sig_command() W_SIGNAL(sig_command);
  void sig_disconnect() W_SIGNAL(sig_disconnect);
  void sig_createDevice() W_SIGNAL(sig_createDevice);

private:
  void slot_command();
  W_SLOT(slot_command);
  void slot_createDevice();
  W_SLOT(slot_createDevice);

  ossia::oscquery::oscquery_mirror_protocol* m_mirror{};
  bool m_connected{};
  Device::DeviceSettings m_oldSettings;
};
}
