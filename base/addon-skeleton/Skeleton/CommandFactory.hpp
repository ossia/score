#pragma once
#include <score/command/Command.hpp>

namespace Skeleton
{
inline const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"Skeleton"};
  return key;
}
}
