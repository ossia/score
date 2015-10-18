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
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), "ClearSelection", "ClearSelection")
            public:
                ClearSelection():
                      AggregateCommand{factoryName(),
                                       commandName(),
                                       description()} { }

        };
    }
}
