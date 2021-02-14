#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{

class WiimoteDevice final : public Device::OwningDeviceInterface
{

  W_OBJECT(WiimoteDevice)
public:
  WiimoteDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx);
  ~WiimoteDevice();

  bool reconnect() override;
  void disconnect() override;

private:
  const score::DocumentContext& m_ctx;
};

}
