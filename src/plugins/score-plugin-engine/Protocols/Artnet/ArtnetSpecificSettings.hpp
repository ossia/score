#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)

#include <verdigris>

namespace Protocols
{

struct ArtnetSpecificSettings
{
  int dummy;
};
}

Q_DECLARE_METATYPE(Protocols::ArtnetSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::ArtnetSpecificSettings)
#endif
