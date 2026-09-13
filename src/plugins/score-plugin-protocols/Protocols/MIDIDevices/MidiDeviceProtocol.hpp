#pragma once

/**
 * @file The ossia protocol that turns a `.midimap.json` device description into
 * a device tree: one node per control, addressed by the message the description
 * gives it and grouped by its groups.
 *
 *     MPK 225/Solos/Button 1
 *     MPK 225/Pandirection/Encoder 3
 *     Volca Keys/Factory/Program
 *     Volca Keys/Factory/Program/choice      (when the values are named)
 *
 * Writing a node sends the message; receiving the message updates the node.
 *
 * @see Protocols/MIDIDevices/MidiDeviceMap.hpp for the format and its reader.
 */

#include <Protocols/MIDIDevices/MidiDeviceMap.hpp>

#include <ossia/network/base/protocol.hpp>

#include <libremidi/api.hpp>
#include <libremidi/observer_configuration.hpp>

#include <memory>
#include <optional>

namespace Protocols::MIDIDevices
{

struct ProtocolSettings
{
  libremidi::API api{};

  /**
   * Either may be unset: output only drives the device, input only follows its
   * knobs, neither is refused. A control's access mode is the narrower of what
   * it declares and what the open ports allow.
   */
  std::optional<libremidi::input_port> input;
  std::optional<libremidi::output_port> output;

  /**
   * The channel, 1-16, for a control whose description states none -- an
   * instrument documents its parameters without one, because the channel is
   * whatever the user set on the front panel.
   */
  int channel{1};

  DeviceMap map;
};

//! Throws std::runtime_error when the ports cannot be opened or the settings
//! describe nothing to talk to. The tree is built in set_device().
std::unique_ptr<ossia::net::protocol_base> makeProtocol(ProtocolSettings settings);

}
