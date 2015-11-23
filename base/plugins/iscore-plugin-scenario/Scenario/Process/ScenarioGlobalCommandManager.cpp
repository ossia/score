#include "ScenarioGlobalCommandManager.hpp"
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

#include <Scenario/Commands/Scenario/Deletions/ClearConstraint.hpp>
#include <Scenario/Commands/Scenario/Deletions/ClearEvent.hpp>

#include <Scenario/Commands/ClearSelection.hpp>
#include <Scenario/Document/Event/EventModel.hpp>

#include <Scenario/Commands/Scenario/Deletions/RemoveSelection.hpp>

using namespace Scenario::Command;
using namespace iscore::IDocument; // for ::path
void Scenario::clearContentFromSelection(const ScenarioModel &scenario, iscore::CommandStack& stack)
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

void Scenario::removeSelection(const ScenarioModel &scenario, iscore::CommandStack& stack)
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
        CommandDispatcher<> dispatcher(stack);
        dispatcher.submitCommand(new RemoveSelection(path(scenario), sel));
    }
}

void Scenario::removeSelection(const BaseScenario&, iscore::CommandStack&)
{
    // Shall do nothing
}

void Scenario::clearContentFromSelection(const BaseScenario&, iscore::CommandStack&)
{
    ISCORE_TODO;
}
