#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

#include <ossia/network/base/device.hpp>

#include <verdigris>

namespace ossia::oscquery_asio
{
class oscquery_mirror_asio_protocol;
}
namespace Protocols
{
class OSCQueryDevice final : public Device::OwningDeviceInterface
{
  W_OBJECT(OSCQueryDevice)
public:
  OSCQueryDevice(
      const Device::DeviceSettings& settings,
      const ossia::net::network_context_ptr& ctx);

  ~OSCQueryDevice();

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

  using mirror_proto = ossia::oscquery_asio::oscquery_mirror_asio_protocol;
  mirror_proto* m_mirror{};
  bool m_connected{};
  Device::DeviceSettings m_oldSettings;
  const ossia::net::network_context_ptr& m_ctx;
};
}
