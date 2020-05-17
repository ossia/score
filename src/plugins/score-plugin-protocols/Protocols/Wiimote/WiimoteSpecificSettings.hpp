#pragma once

#include <verdigris>

namespace Protocols
{

struct WiimoteSpecificSettings
{
  int dummy;
};

}

Q_DECLARE_METATYPE(Protocols::WiimoteSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::WiimoteSpecificSettings)
