#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/AggregateCommand.hpp>

namespace Scenario
{
class SnapshotStatesMacro final : public iscore::AggregateCommand
{
  ISCORE_COMMAND_DECL(
      Scenario::Command::ScenarioCommandFactoryName(), SnapshotStatesMacro,
      "SnapshotStatesMacro")
};
}
