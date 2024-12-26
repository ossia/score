#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{
class MCUDevice final : public Device::OwningDeviceInterface
{
public:
  MCUDevice(
      const Device::DeviceSettings& settings, const ossia::net::network_context_ptr& ctx,
      const score::DocumentContext& doc);
  ~MCUDevice();

  bool reconnect() override;
  void disconnect() override;

private:
  const ossia::net::network_context_ptr& m_ctx;
  const score::DocumentContext& m_doc;
};
}
