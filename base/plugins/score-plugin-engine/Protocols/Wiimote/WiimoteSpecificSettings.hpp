#pragma once

#include <QMetaType>
#include <wobjectdefs.h>

namespace Engine::Network
{

struct WiimoteSpecificSettings
{
  int dummy;
};
}

Q_DECLARE_METATYPE(Engine::Network::WiimoteSpecificSettings)
W_REGISTER_ARGTYPE(Engine::Network::WiimoteSpecificSettings)
