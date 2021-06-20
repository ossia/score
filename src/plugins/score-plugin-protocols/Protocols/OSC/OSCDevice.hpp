#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{
class OSCDevice final : public Device::OwningDeviceInterface
{
public:
  OSCDevice(
      const Device::DeviceSettings& stngs,
      const ossia::net::network_context_ptr& ctx);

  bool reconnect() override;
  void recreate(const Device::Node&) final override;

  bool isLearning() const final override;
  void setLearning(bool) final override;

private:
  const ossia::net::network_context_ptr& m_ctx;
};
}
