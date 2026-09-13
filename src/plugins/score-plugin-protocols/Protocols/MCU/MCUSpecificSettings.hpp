#pragma once
#include <QString>

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
   * The device map, as "ardour/donnerdmk25.midimap.json". Not a path: a score
   * has to keep pointing at the same description across a package update, a
   * different machine, or a user's own copy of the maps.
   */
  QString map;

  //! The channel, 1-16, the instrument is set to: the dataset documents
  //! parameter CCs without one.
  int channel{1};
};
}
Q_DECLARE_METATYPE(Protocols::MCUSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::MCUSpecificSettings)
