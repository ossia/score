#pragma once
#include <iscore/command/AggregateCommand.hpp>
namespace Scenario
{
    namespace Command
    {
        class CreationMetaCommand : public iscore::AggregateCommand
        {
                ISCORE_COMMAND
            public:
                CreationMetaCommand():
                  AggregateCommand{"ScenarioControl",
                                   commandName(),
                                   description()} { }
        };
    }
}
