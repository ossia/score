#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

#include <ossia/network/zeroconf/zeroconf.hpp>

namespace ossia::net
{
class protocol_base;
template <typename T>
class can_learn;
using osc_protocol_base = can_learn<protocol_base>;
}

namespace Protocols
{
struct OSCSpecificSettings;
class OSCDevice final : public Device::OwningDeviceInterface
{
public:
  OSCDevice(
      const Device::DeviceSettings& stngs, const ossia::net::network_context_ptr& ctx);

  void disconnect() override;
  bool reconnect() override;
  void recreate(const Device::Node&) final override;

  bool isLearning() const final override;
  void setLearning(bool) final override;

private:
  void setup_zeroconf(const OSCSpecificSettings& stgs, const std::string& name);
  const ossia::net::network_context_ptr& m_ctx;
  ossia::net::zeroconf_server m_zeroconf{};
  ossia::net::osc_protocol_base* m_oscproto{};
};
}
