#pragma once

#include <iscore/command/AggregateCommand.hpp>
#include <QVector>

namespace Scenario
{
    namespace Command
    {
        class ClearSelection : public iscore::AggregateCommand
        {
                ISCORE_COMMAND_DECL("ClearSelection", "ClearSelection")
            public:
                ClearSelection():
                      AggregateCommand{"ScenarioControl",
                                       commandName(),
                                       description()} { }

        };
    }
}
