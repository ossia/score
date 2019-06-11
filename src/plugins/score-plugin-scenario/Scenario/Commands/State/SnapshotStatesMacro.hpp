#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/AggregateCommand.hpp>

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
