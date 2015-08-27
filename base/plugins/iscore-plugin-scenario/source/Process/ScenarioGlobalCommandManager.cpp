#include "ScenarioGlobalCommandManager.hpp"
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

#include "Commands/Scenario/Deletions/ClearConstraint.hpp"
#include "Commands/Scenario/Deletions/ClearEvent.hpp"

#include "Commands/ClearSelection.hpp"
#include "Document/Event/EventModel.hpp"

#include "Commands/Scenario/Deletions/RemoveSelection.hpp"

#include <iscore/tools/utilsCPP11.hpp>
using namespace Scenario::Command;
using namespace iscore::IDocument; // for ::path
void ScenarioGlobalCommandManager::clearContentFromSelection(const ScenarioModel &scenario)
{
    // 1. Select items
    auto constraintsToRemove = selectedElements(scenario.constraints());
    auto statesToRemove = selectedElements(scenario.states());

    MacroCommandDispatcher cleaner(new ClearSelection,
                                   m_commandStack);

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

void ScenarioGlobalCommandManager::removeSelection(const ScenarioModel &scenario)
{
    Selection sel = scenario.selectedChildren();

    // We have to remove the first / last timenodes / events from the selection.
    erase_if(sel, [&] (auto&& elt) {
        return elt == &scenario.startEvent()
            || elt == &scenario.endEvent()
            || elt == &scenario.startTimeNode()
            || elt == &scenario.endTimeNode();
    });

    if(!sel.empty())
    {
        CommandDispatcher<> dispatcher(m_commandStack);
        dispatcher.submitCommand(new RemoveSelection(path(scenario), sel));
    }
}
