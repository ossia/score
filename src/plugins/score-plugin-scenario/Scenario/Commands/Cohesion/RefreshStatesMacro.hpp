#pragma once
#include <score/command/AggregateCommand.hpp>

#include <QObject>

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
namespace Scenario
{
namespace Command
{
class RefreshStatesMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      RefreshStatesMacro,
      "Refresh states")
};
}
}
