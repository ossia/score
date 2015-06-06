#include "ScenarioGlobalCommandManager.hpp"
#include <iscore/command/OngoingCommandManager.hpp>

#include "Commands/Scenario/Deletions/RemoveConstraint.hpp"
#include "Commands/Scenario/Deletions/RemoveEvent.hpp"
#include "Commands/Scenario/Deletions/ClearConstraint.hpp"
#include "Commands/Scenario/Deletions/ClearEvent.hpp"

#include "Commands/RemoveMultipleElements.hpp"
#include "Document/Event/EventModel.hpp"


using namespace Scenario::Command;
using namespace iscore::IDocument; // for ::path
void ScenarioGlobalCommandManager::clearContentFromSelection(const ScenarioModel &scenario)
{
    // 1. Select items
    // TODO timenode ?
    auto constraintsToRemove = selectedElements(scenario.constraints());
    auto eventsToRemove = selectedElements(scenario.events());

    MacroCommandDispatcher cleaner(new RemoveMultipleElements,
                                   m_commandStack);

    // 3. Create a Delete command for each. For now : only emptying.
    for(auto& constraint : constraintsToRemove)
    {
        cleaner.submitCommand(new ClearConstraint(path(constraint)));
    }

    for(auto& event : eventsToRemove)
    {
        cleaner.submitCommand(new ClearEvent(path(event)));
    }

    cleaner.commit();
}

#include "Commands/Scenario/Deletions/RemoveSelection.hpp"
void ScenarioGlobalCommandManager::removeSelection(const ScenarioModel &scenario)
{
    auto sel = scenario.selectedChildren();
    if(!sel.empty())
    {
        CommandDispatcher<> dispatcher(m_commandStack);
        dispatcher.submitCommand(new RemoveSelection(path(scenario), sel));
    }
}
