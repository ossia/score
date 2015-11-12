#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/AggregateCommand.hpp>
#include <QVector>

namespace Scenario
{
    namespace Command
    {
        class ClearSelection final : public iscore::AggregateCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), ClearSelection, "ClearSelection")
        };
    }
}
