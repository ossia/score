#pragma once

#include <Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/AggregateCommand.hpp>
#include <QVector>

namespace Scenario
{
    namespace Command
    {
        class ClearSelection : public iscore::AggregateCommand
        {
                ISCORE_AGGREGATE_COMMAND_DECL(ScenarioCommandFactoryName(), ClearSelection, "ClearSelection")
        };
    }
}
