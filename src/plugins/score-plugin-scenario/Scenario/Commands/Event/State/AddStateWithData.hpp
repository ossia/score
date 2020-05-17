#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/AggregateCommand.hpp>

namespace Scenario
{
namespace Command
{
class AddStateWithData final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), AddStateWithData, "Drop a new state in an event")
};
}
}
