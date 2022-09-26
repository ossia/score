#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/AggregateCommand.hpp>
namespace Scenario
{
namespace Command
{

class ReplaceAddresses : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), ReplaceAddresses, "Replace addresses");
};
}
}
