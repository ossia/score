#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{
class MIDIDevice final : public Device::OwningDeviceInterface
{
public:
  MIDIDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx);

  bool reconnect() override;

  void disconnect() override;

  QMimeData* mimeData() const override;

  using OwningDeviceInterface::refresh;
  Device::Node refresh() override;

  bool isLearning() const final override;
  void setLearning(bool) final override;

private:
  const score::DocumentContext& m_ctx;
};
}
