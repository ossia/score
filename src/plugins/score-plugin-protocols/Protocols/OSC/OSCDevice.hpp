#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{
class OSCDevice final : public Device::OwningDeviceInterface
{
public:
  OSCDevice(const Device::DeviceSettings& stngs, const score::DocumentContext& ctx);

  bool reconnect() override;
  void recreate(const Device::Node&) final override;

  bool isLearning() const final override;
  void setLearning(bool) final override;
private:
  const score::DocumentContext& m_ctx;
};
}
