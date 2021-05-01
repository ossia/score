#pragma once
#include <score/command/AggregateCommand.hpp>

#include <Scenario/Commands/ScenarioCommandFactory.hpp>

namespace Scenario
{
class SnapshotStatesMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      Scenario::Command::CommandFactoryName(),
      SnapshotStatesMacro,
      "SnapshotStatesMacro")
};
}
