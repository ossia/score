#pragma once
#include <score/command/Command.hpp>

namespace Patternist
{
inline const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"Patternist"};
  return key;
}
}
