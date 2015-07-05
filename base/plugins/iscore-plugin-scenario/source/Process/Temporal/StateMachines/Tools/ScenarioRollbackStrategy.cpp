#include "ScenarioRollbackStrategy.hpp"
#include "Commands/Scenario/Creations/CreateConstraint.hpp"
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
        if(cmds[i]->name() == CreateConstraint::commandName()
                || cmds[i]->name() == CreateEventAfterEvent::commandName()
                || cmds[i]->name() == CreateEventAfterEventOnTimeNode::commandName()
                || cmds[i]->name() == CreateEventOnTimeNode::commandName())
        {
            cmds[i]->undo();
        }
    }
}
