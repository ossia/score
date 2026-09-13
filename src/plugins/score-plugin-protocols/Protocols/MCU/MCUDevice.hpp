#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

#include <memory>

namespace ossia::net
{
class protocol_base;
}

namespace Protocols
{
struct MCUSpecificSettings;

/**
 * The MIDI Controller device. Two unrelated things behind one protocol, chosen
 * by MCUSpecificSettings::mode: a Mackie Control surface mapped onto score's
 * remote control interface, or the devices a `.midimap.json` description names,
 * whose controls become the device's tree.
 */
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
  //! Null when the settings describe nothing openable.
  std::unique_ptr<ossia::net::protocol_base>
  makeMCUProtocol(const MCUSpecificSettings& set);

  std::unique_ptr<ossia::net::protocol_base>
  makeMidiDeviceMapProtocol(const MCUSpecificSettings& set);

  const ossia::net::network_context_ptr& m_ctx;
  const score::DocumentContext& m_doc;
};
}
