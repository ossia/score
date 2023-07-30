#pragma once
#include <QString>

#include <libremidi/api.hpp>
#include <libremidi/observer_configuration.hpp>

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
  libremidi::API api{};
  bool virtualPort{};

  bool createWholeTree{};
};
}
Q_DECLARE_METATYPE(Protocols::MIDISpecificSettings)
W_REGISTER_ARGTYPE(Protocols::MIDISpecificSettings)
