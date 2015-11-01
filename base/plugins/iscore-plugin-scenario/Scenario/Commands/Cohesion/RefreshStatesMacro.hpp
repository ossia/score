#pragma once
#include <iscore/command/AggregateCommand.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

class RefreshStatesMacro : public iscore::AggregateCommand
{
        ISCORE_AGGREGATE_COMMAND_DECL(ScenarioCommandFactoryName(),
                                      RefreshStatesMacro,
                                      "RefreshStatesMacro")
};
