// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LoopCommandFactory.hpp"

#include <score/command/Command.hpp>

const CommandGroupKey& LoopCommandFactoryName()
{
  static const CommandGroupKey key{"Loop"};
  return key;
}

namespace Loop
{
class ProcessModel;
} // namespace Loop

template <>
const CommandGroupKey& CommandFactoryName<Loop::ProcessModel>()
{
  return LoopCommandFactoryName();
}
