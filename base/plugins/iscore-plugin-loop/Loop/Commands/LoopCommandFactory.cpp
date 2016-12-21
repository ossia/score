#include "LoopCommandFactory.hpp"
#include <iscore/command/Command.hpp>

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
