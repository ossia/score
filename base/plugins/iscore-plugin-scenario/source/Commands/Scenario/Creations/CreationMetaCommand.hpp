#pragma once
#include <iscore/command/AggregateCommand.hpp>
#include "CreateConstraint.hpp"
#include "CreateEventAfterEvent.hpp"
#include "CreateEventAfterEventOnTimeNode.hpp"
#include "CreateEventOnTimeNode.hpp"
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

                virtual void undo() override
        {
            // We only undo the creation commands
            // since the move ones perform unnecessary serialization / etc in this case
            // and don't bring anything to the table.
            for(int i = m_cmds.size() - 1; i >= 0; --i)
            {
                if(m_cmds[i]->name() == CreateConstraint::commandName()
                || m_cmds[i]->name() == CreateEventAfterEvent::commandName()
                || m_cmds[i]->name() == CreateEventAfterEventOnTimeNode::commandName()
                || m_cmds[i]->name() == CreateEventOnTimeNode::commandName())
                {
                    m_cmds[i]->undo();
                }
            }
        }
        };
    }
}
