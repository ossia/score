#pragma once

#include <QMetaType>

#include <wobjectdefs.h>

namespace Protocols
{

struct ArtnetSpecificSettings
{
  int dummy;
};
}

Q_DECLARE_METATYPE(Protocols::ArtnetSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::ArtnetSpecificSettings)
