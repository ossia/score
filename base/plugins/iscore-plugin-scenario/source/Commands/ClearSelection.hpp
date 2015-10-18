#pragma once

#include <iscore/command/AggregateCommand.hpp>
#include <QVector>

namespace Scenario
{
    namespace Command
    {
        class ClearSelection : public iscore::AggregateCommand
        {
                ISCORE_COMMAND_DECL("ScenarioControl", "ClearSelection", "ClearSelection")
            public:
                ClearSelection():
                      AggregateCommand{factoryName(),
                                       commandName(),
                                       description()} { }

        };
    }
}
