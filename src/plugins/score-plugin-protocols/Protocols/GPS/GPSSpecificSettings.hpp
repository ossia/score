#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_GPS)
#include <score/tools/std/StringHash.hpp>

#include <ossia/detail/variant.hpp>

#include <QString>

#include <utility>
#include <vector>
#include <verdigris>

namespace Protocols
{
struct GPSSpecificSettings
{
  QString host;
  int port{};
};
}

Q_DECLARE_METATYPE(Protocols::GPSSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::GPSSpecificSettings)
#endif
