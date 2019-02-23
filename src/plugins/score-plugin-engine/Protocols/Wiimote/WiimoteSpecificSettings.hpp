#pragma once

#include <QMetaType>

#include <wobjectdefs.h>

namespace Protocols
{

struct WiimoteSpecificSettings
{
  int dummy;
};

}

Q_DECLARE_METATYPE(Protocols::WiimoteSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::WiimoteSpecificSettings)
