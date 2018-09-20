#pragma once

#include <QMetaType>
#include <wobjectdefs.h>

namespace Engine::Network {

struct ArtnetSpecificSettings
{
  int dummy;
};

}

Q_DECLARE_METATYPE(Engine::Network::ArtnetSpecificSettings)
W_REGISTER_ARGTYPE(Engine::Network::ArtnetSpecificSettings)
