#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

#include <ossia/network/zeroconf/zeroconf.hpp>

namespace Protocols
{
class BitfocusDevice final : public Device::OwningDeviceInterface
{
public:
  BitfocusDevice(
      const Device::DeviceSettings& stngs, const ossia::net::network_context_ptr& ctx);

  bool reconnect() override;

  bool isLearning() const final override;
  void setLearning(bool) final override;

private:
  const ossia::net::network_context_ptr& m_ctx;
};
}
