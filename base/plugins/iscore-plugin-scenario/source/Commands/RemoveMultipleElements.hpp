#pragma once

#include <iscore/command/AggregateCommand.hpp>
#include <QVector>

namespace Scenario
{
    namespace Command
    {
        class RemoveMultipleElements : public iscore::AggregateCommand
        {
                ISCORE_COMMAND_DECL("RemoveMultipleElements", "RemoveMultipleElements")
            public:
                RemoveMultipleElements():
                      AggregateCommand{"ScenarioControl",
                                       commandName(),
                                       description()} { }

        };
    }
}
