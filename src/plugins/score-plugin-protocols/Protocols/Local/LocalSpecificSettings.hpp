#pragma once

#include <verdigris>

namespace Protocols
{
struct LocalSpecificSettings
{
  int wsPort{};
  int oscPort{};
};
}
Q_DECLARE_METATYPE(Protocols::LocalSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::LocalSpecificSettings)
