#pragma once
#include <QString>

#include <vector>

#include <libremidi/api.hpp>
#include <libremidi/observer_configuration.hpp>

#include <verdigris>

namespace Protocols
{
struct MCUSpecificSettings
{
  std::vector<libremidi::input_port> input_handle;
  std::vector<libremidi::output_port> output_handle;
  libremidi::API api{};


  enum Mode
  {
    //! Mapped onto score's remote control interface; no node tree of its own.
    MCU,

    //! A tree built from a `.midimap.json` description: a controller's knobs
    //! and pads, or an instrument's parameters.
    //! @see Protocols/MIDIDevices/MidiDeviceProtocol.hpp
    MidiDeviceMap
  } mode{MCU};

  /**
   * One device on the port, and the channel it is set to.
   *
   * The map is named as "ardour/donnerdmk25.midimap.json" rather than by path:
   * a score has to keep pointing at the same description across a package
   * update, a different machine, or a user's own copy of the maps.
   */
  struct MapSlot
  {
    QString map;
    int channel{1};

    bool operator==(const MapSlot&) const noexcept = default;
  };

  /**
   * A MIDI cable carries sixteen channels, so one port can reach several
   * instruments at once. Each is a description of its own, on its own channel.
   */
  std::vector<MapSlot> maps;
};
}
Q_DECLARE_METATYPE(Protocols::MCUSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::MCUSpecificSettings)
