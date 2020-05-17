#pragma once
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>

#include <score/command/AggregateCommand.hpp>

namespace Explorer
{
namespace Command
{
class RemoveNodes final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), RemoveNodes, "Remove Explorer nodes")
};
}
}
