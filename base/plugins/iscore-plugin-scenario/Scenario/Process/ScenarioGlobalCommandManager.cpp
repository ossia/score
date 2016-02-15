#include <Scenario/Commands/ClearSelection.hpp>
#include <Scenario/Commands/Scenario/Deletions/ClearConstraint.hpp>
#include <Scenario/Commands/Scenario/Deletions/ClearState.hpp>
#include <Scenario/Commands/Scenario/Deletions/RemoveSelection.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <QDebug>
#include <algorithm>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include "ScenarioGlobalCommandManager.hpp"
#include <iscore/selection/Selection.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/utilsCPP11.hpp>

namespace iscore {
class CommandStackFacade;
}  // namespace iscore

using namespace Scenario::Command;
using namespace iscore::IDocument; // for ::path
void Scenario::clearContentFromSelection(
        const Scenario::ScenarioModel& scenario,
        iscore::CommandStackFacade& stack)
{
    // 1. Select items
    auto constraintsToRemove = selectedElements(scenario.constraints);
    auto statesToRemove = selectedElements(scenario.states);

    MacroCommandDispatcher cleaner(new ClearSelection,
                                   stack);

    // 2. Create a Clear command for each.
    for(auto& constraint : constraintsToRemove)
    {
        cleaner.submitCommand(new ClearConstraint(path(*constraint)));
    }

    for(auto& state : statesToRemove)
    {
        cleaner.submitCommand(new ClearState(path(*state)));
    }

    cleaner.commit();
}


void Scenario::removeSelection(
        const Scenario::ScenarioModel& scenario,
        iscore::CommandStackFacade& stack)
{
    Selection sel = scenario.selectedChildren();

    // We have to remove the first / last timenodes / events from the selection.
    erase_if(sel, [&] (auto&& elt) {
        return elt->id_val() == startId_val();
    });

    if(!sel.empty())
    {
        CommandDispatcher<> dispatcher(stack);
        dispatcher.submitCommand(new RemoveSelection(path(scenario), sel));
    }
}

void Scenario::removeSelection(
        const BaseScenario&,
        iscore::CommandStackFacade&)
{
    // Shall do nothing
}

void Scenario::clearContentFromSelection(
        const BaseScenario&,
        iscore::CommandStackFacade&)
{
    ISCORE_TODO;
}
