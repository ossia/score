#pragma once
#include <score/command/Command.hpp>

namespace Gfx
{
inline const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"gfx"};
  return key;
}
}
