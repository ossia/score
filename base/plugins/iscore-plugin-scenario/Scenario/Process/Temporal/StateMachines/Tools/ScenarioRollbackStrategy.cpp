#include "ScenarioRollbackStrategy.hpp"

#include <Scenario/Commands/Scenario/Creations/CreateState.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateEvent_State.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateConstraint.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateConstraint_State.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateConstraint_State_Event.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateConstraint_State_Event_TimeNode.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateSequence.hpp>


void ScenarioRollbackStrategy::rollback(const QList<iscore::SerializableCommand *> &cmds)
{
    // TODO UPDATE THIS ELSE ROLLBACK WON'T WORK.
    // REFACTOR THIS IN A LIST SOMEWHERE.
    using namespace Scenario::Command;
    for(int i = cmds.size() - 1; i >= 0; --i)
    {
        if(
                cmds[i]->uid() == CreateConstraint::static_uid()
                || cmds[i]->uid() == CreateState::static_uid()
                || cmds[i]->uid() == CreateEvent_State::static_uid()
                || cmds[i]->uid() == CreateConstraint_State::static_uid()
                || cmds[i]->uid() == CreateConstraint_State_Event::static_uid()
                || cmds[i]->uid() == CreateConstraint_State_Event_TimeNode::static_uid()
                || cmds[i]->uid() == CreateSequence::static_uid()
                )
        {
            cmds[i]->undo();
        }
    }
}
