#pragma once
#include <score/command/Command.hpp>
namespace Jit
{
inline const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"Jit"};
  return key;
}

}
