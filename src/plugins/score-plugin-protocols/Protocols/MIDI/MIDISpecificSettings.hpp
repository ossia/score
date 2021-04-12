#pragma once
#include <libremidi/api.hpp>

#include <QString>

#include <verdigris>

namespace Protocols
{
struct MIDISpecificSettings
{
  enum class IO
  {
    In,
    Out
  } io{};
  QString endpoint;
  int port{};
  libremidi::API api{};
  bool createWholeTree{};
};
}
Q_DECLARE_METATYPE(Protocols::MIDISpecificSettings)
W_REGISTER_ARGTYPE(Protocols::MIDISpecificSettings)
