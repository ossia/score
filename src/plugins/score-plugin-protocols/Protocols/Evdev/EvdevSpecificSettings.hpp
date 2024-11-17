#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_EVDEV)
#include <score/tools/std/StringHash.hpp>

#include <ossia/detail/variant.hpp>

#include <QString>

#include <utility>
#include <vector>
#include <verdigris>

namespace Protocols
{
struct EvdevSpecificSettings
{
  QString name;
  QString handler;
  QString bus, vendor, product, version;
};
}

Q_DECLARE_METATYPE(Protocols::EvdevSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::EvdevSpecificSettings)
#endif
