#pragma once
#include <QJsonObject>
#include <QMetaType>

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
  bool createWholeTree{};
};
}
Q_DECLARE_METATYPE(Protocols::MIDISpecificSettings)
W_REGISTER_ARGTYPE(Protocols::MIDISpecificSettings)
