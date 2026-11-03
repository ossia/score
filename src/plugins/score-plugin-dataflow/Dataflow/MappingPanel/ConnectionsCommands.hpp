#pragma once
#include <score/command/AggregateCommand.hpp>
#include <score/command/Command.hpp>

namespace Dataflow
{
inline const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"Dataflow"};
  return key;
}

//! Removes cables and unbinds port addresses in one undo step
class RemoveConnections final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(Dataflow::CommandFactoryName(), RemoveConnections, "Remove connections")
};
}
