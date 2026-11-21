#pragma once
#include <QString>

#include <libremidi/api.hpp>
#include <libremidi/observer_configuration.hpp>
#include <libremidi/types.hpp>

#include <verdigris>

namespace Protocols
{
struct MIDISpecificSettings
{
  libremidi::port_information handle;

  enum class IO
  {
    In,
    Out
  } io{};
  bool virtualPort{};

  bool createWholeTree{};

  bool velocityZeroIsNoteOff{};

  //! For an input: whether bank select, data entry and RPN / NRPN controllers
  //! reach the document as the control changes the device sent (raw) or as
  //! the MIDI 2 messages they make together (semantic).
  libremidi::controller_translation controllers{libremidi::controller_translation::raw};
};
}
Q_DECLARE_METATYPE(Protocols::MIDISpecificSettings)
W_REGISTER_ARGTYPE(Protocols::MIDISpecificSettings)
