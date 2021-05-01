#pragma once
#include <score/command/AggregateCommand.hpp>

#include <Scenario/Commands/ScenarioCommandFactory.hpp>

namespace Scenario
{
namespace Command
{
class AddStateWithData final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      AddStateWithData,
      "Drop a new state in an event")
};
}
}
