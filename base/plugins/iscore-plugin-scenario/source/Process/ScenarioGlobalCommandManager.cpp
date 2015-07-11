#include "ScenarioGlobalCommandManager.hpp"
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

#include "Commands/Scenario/Deletions/ClearConstraint.hpp"
#include "Commands/Scenario/Deletions/ClearEvent.hpp"

#include "Commands/RemoveMultipleElements.hpp"
#include "Document/Event/EventModel.hpp"

#include "Commands/Scenario/Deletions/RemoveSelection.hpp"

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
        cleaner.submitCommand(new ClearConstraint(path(constraint)));
    }

    for(auto& state : statesToRemove)
    {
        cleaner.submitCommand(new ClearState(path(state)));
    }

    cleaner.commit();
}

void ScenarioGlobalCommandManager::removeSelection(const ScenarioModel &scenario)
{
    auto sel = scenario.selectedChildren();
    if(!sel.empty())
    {
        CommandDispatcher<> dispatcher(m_commandStack);
        dispatcher.submitCommand(new RemoveSelection(path(scenario), sel));
    }
}
