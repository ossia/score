#pragma once
#include <iscore/command/AggregateCommand.hpp>

#include "CreateState.hpp"
#include "CreateEvent_State.hpp"

#include "CreateConstraint.hpp"
#include "CreateConstraint_State.hpp"
#include "CreateConstraint_State_Event.hpp"
#include "CreateConstraint_State_Event_TimeNode.hpp"
#include "CreateSequence.hpp"
#include <boost/range/adaptor/reversed.hpp>
namespace Scenario
{
    namespace Command
    {
        class CreationMetaCommand final : public iscore::AggregateCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), CreationMetaCommand, "Create elements in scenario")
            public:
                void undo() const override
        {
            // We only undo the creation commands
            // since the move ones perform unnecessary serialization / etc in this case
            // and don't bring anything to the table.
            // TODO REFACTOR WITH SCENARIOROLLBACKSTRATEGY
            for (auto cmd : boost::adaptors::reverse(m_cmds))
            {
                if(
                        cmd->key() == CreateConstraint::static_key()
                        || cmd->key() == CreateState::static_key()
                        || cmd->key() == CreateEvent_State::static_key()
                        || cmd->key() == CreateConstraint_State::static_key()
                        || cmd->key() == CreateConstraint_State_Event::static_key()
                        || cmd->key() == CreateConstraint_State_Event_TimeNode::static_key()
                        || cmd->key() == CreateSequence::static_key()
                        )
                {
                    cmd->undo();
                }
            }
        }
        };
    }
}
