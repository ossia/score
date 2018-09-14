#pragma once
#include <QJsonObject>
#include <QMetaType>

#include <wobjectdefs.h>

namespace Engine
{
namespace Network
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
};
}
}
Q_DECLARE_METATYPE(Engine::Network::MIDISpecificSettings)
W_REGISTER_ARGTYPE(Engine::Network::MIDISpecificSettings)
