#pragma once
#include <iscore/command/AggregateCommand.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

namespace Scenario
{
class SnapshotStatesMacro final : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL(Scenario::Command::ScenarioCommandFactoryName(), SnapshotStatesMacro, "SnapshotStatesMacro")
};
}
