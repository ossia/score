#pragma once
#include <iscore/command/AggregateCommand.hpp>

#include "CreateState.hpp"
#include "CreateEvent_State.hpp"

#include "CreateConstraint.hpp"
#include "CreateConstraint_State.hpp"
#include "CreateConstraint_State_Event.hpp"
#include "CreateConstraint_State_Event_TimeNode.hpp"
namespace Scenario
{
    namespace Command
    {
        class CreationMetaCommand : public iscore::AggregateCommand
        {
                ISCORE_COMMAND_DECL("CreationMetaCommand", "CreationMetaCommand")
            public:
                CreationMetaCommand():
                  AggregateCommand{"ScenarioControl",
                                   commandName(),
                                   description()} { }

                virtual void undo() override
        {
            // We only undo the creation commands
            // since the move ones perform unnecessary serialization / etc in this case
            // and don't bring anything to the table.
            for(int i = m_cmds.size() - 1; i >= 0; --i)
            {
                if(
                        m_cmds[i]->uid() == CreateConstraint::static_uid()
                        || m_cmds[i]->uid() == CreateState::static_uid()
                        || m_cmds[i]->uid() == CreateEvent_State::static_uid()
                        || m_cmds[i]->uid() == CreateConstraint_State::static_uid()
                        || m_cmds[i]->uid() == CreateConstraint_State_Event::static_uid()
                        || m_cmds[i]->uid() == CreateConstraint_State_Event_TimeNode::static_uid()
                        )
                {
                    m_cmds[i]->undo();
                }
            }
        }
        };
    }
}
