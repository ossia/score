#pragma once
#include <score/command/Command.hpp>

namespace ControlSurface
{
inline const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"ControlSurface"};
  return key;
}
}
