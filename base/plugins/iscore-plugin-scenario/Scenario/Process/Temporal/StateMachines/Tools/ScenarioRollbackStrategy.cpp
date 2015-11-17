#include "ScenarioRollbackStrategy.hpp"

#include <Scenario/Commands/Scenario/Creations/CreateState.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateEvent_State.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateConstraint.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateConstraint_State.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateConstraint_State_Event.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateConstraint_State_Event_TimeNode.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateSequence.hpp>


void ScenarioRollbackStrategy::rollback(const std::vector<iscore::SerializableCommand *> &cmds)
{
    // TODO UPDATE THIS ELSE ROLLBACK WON'T WORK.
    // REFACTOR THIS IN A LIST SOMEWHERE.
    using namespace Scenario::Command;
    for(int i = cmds.size() - 1; i >= 0; --i)
    {
        if(
                cmds[i]->key() == CreateConstraint::static_key()
                || cmds[i]->key() == CreateState::static_key()
                || cmds[i]->key() == CreateEvent_State::static_key()
                || cmds[i]->key() == CreateConstraint_State::static_key()
                || cmds[i]->key() == CreateConstraint_State_Event::static_key()
                || cmds[i]->key() == CreateConstraint_State_Event_TimeNode::static_key()
                || cmds[i]->key() == CreateSequence::static_key()
                )
        {
            cmds[i]->undo();
        }
    }
}
