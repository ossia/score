#pragma once
#include <score/command/Command.hpp>

namespace Nodal
{
inline const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"Nodal"};
  return key;
}
}
