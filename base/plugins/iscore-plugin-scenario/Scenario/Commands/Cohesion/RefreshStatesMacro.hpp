#pragma once
#include <iscore/command/AggregateCommand.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

namespace Scenario
{
class RefreshStatesMacro final : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), RefreshStatesMacro, "Refresh states")
};
}
