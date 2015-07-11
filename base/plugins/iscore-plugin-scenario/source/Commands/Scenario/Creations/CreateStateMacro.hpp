#pragma once
#include <iscore/command/AggregateCommand.hpp>

namespace Scenario
{
    namespace Command
    {
        class CreateStateMacro : public iscore::AggregateCommand
        {
                ISCORE_COMMAND_DECL("CreateStateMacro", "CreateStateMacro")
            public:
                CreateStateMacro():
                      AggregateCommand{"ScenarioControl",
                                       commandName(),
                                       description()} { }

        };
    }
}
