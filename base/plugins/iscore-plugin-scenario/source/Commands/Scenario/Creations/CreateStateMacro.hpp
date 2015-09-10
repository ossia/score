#pragma once
#include <iscore/command/AggregateCommand.hpp>

namespace Scenario
{
namespace Command
{
/**
 * @brief The CreateStateMacro class
 * 
 * Used to quickly create a state from data coming from outside.
 * For instance creating a StateModel and adding data inside.
 * 
 */
class CreateStateMacro : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL_OBSOLETE("CreateStateMacro", "CreateStateMacro")
    public:
        CreateStateMacro():
            AggregateCommand{"ScenarioControl",
                             commandName(),
                             description()} { }
        
        void undo() override
        {
            // We only need to undo the creation of the StateModel.
            m_cmds[0]->undo();
        }
        
};
}
}
