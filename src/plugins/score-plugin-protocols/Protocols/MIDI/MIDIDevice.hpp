#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{
class MidiKeyboardEventFilter;
class MIDIDevice final : public Device::OwningDeviceInterface
{
public:
  MIDIDevice(
      const Device::DeviceSettings& settings,
      const ossia::net::network_context_ptr& ctx);
  ~MIDIDevice();

  bool reconnect() override;

  void disconnect() override;

  QMimeData* mimeData() const override;

  using OwningDeviceInterface::refresh;
  Device::Node refresh() override;

  bool isLearning() const final override;
  void setLearning(bool) final override;

private:
  const ossia::net::network_context_ptr& m_ctx;
  MidiKeyboardEventFilter* m_kbdfilter{};
};
}
