#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/AggregateCommand.hpp>

namespace Scenario
{
namespace Command
{
class RefreshStatesMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      RefreshStatesMacro,
      "Refresh states")
};
}
}
