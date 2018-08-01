#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

namespace score
{
struct DeviceSettings;
} // namespace score

namespace Engine
{
namespace Network
{
class MIDIDevice final : public Device::OwningDeviceInterface
{
public:
  MIDIDevice(const Device::DeviceSettings& settings);

  bool reconnect() override;

  void disconnect() override;

  QMimeData* mimeData() const override;

  using OwningDeviceInterface::refresh;
  Device::Node refresh() override;
};
}
}
