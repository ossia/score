#pragma once
#include <iscore/command/AggregateCommand.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

class RefreshStatesMacro final : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(),
                                      RefreshStatesMacro,
                                      "RefreshStatesMacro")
};
