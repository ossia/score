#include "ScenarioRollbackStrategy.hpp"

#include "Commands/Scenario/Creations/CreateState.hpp"
#include "Commands/Scenario/Creations/CreateEvent_State.hpp"
#include "Commands/Scenario/Creations/CreateConstraint.hpp"
#include "Commands/Scenario/Creations/CreateConstraint_State.hpp"
#include "Commands/Scenario/Creations/CreateConstraint_State_Event.hpp"
#include "Commands/Scenario/Creations/CreateConstraint_State_Event_TimeNode.hpp"
#include "Commands/Scenario/Creations/CreateEventAfterEvent.hpp"
#include "Commands/Scenario/Creations/CreateEventAfterEventOnTimeNode.hpp"
#include "Commands/Scenario/Creations/CreateEventOnTimeNode.hpp"

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
                || cmds[i]->uid() == CreateEventAfterEvent::static_uid()
                || cmds[i]->uid() == CreateEventAfterEventOnTimeNode::static_uid()
                || cmds[i]->uid() == CreateEventOnTimeNode::static_uid()
                )
        {
            cmds[i]->undo();
        }
    }
}
