#include "ScenarioGlobalCommandManager.hpp"
#include <iscore/command/OngoingCommandManager.hpp>

#include "Commands/Scenario/RemoveConstraint.hpp"
#include "Commands/Scenario/RemoveEvent.hpp"
#include "Commands/Scenario/ClearConstraint.hpp"
#include "Commands/Scenario/ClearEvent.hpp"

#include "Commands/RemoveMultipleElements.hpp"
#include "Process/ScenarioModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Event/EventModel.hpp"

template<typename Vector>
Vector selectedElements(const Vector& in)
{
    Vector out;
    std::copy_if(begin(in),
                 end(in),
                 back_inserter(out),
                 [](typename Vector::value_type c)
    {
        return c->selection.get();
    });

    return out;
}


using namespace Scenario::Command;
void ScenarioGlobalCommandManager::clearContentFromSelection(const ScenarioModel &scenario)
{
    // 1. Select items
    // TODO timenode ?
    auto constraintsToRemove = selectedElements(scenario.constraints());
    auto eventsToRemove = selectedElements(scenario.events());

    MacroCommandDispatcher cleaner(new RemoveMultipleElements,
                                   m_commandStack,
                                   nullptr);

    // 3. Create a Delete command for each. For now : only emptying.
    for(auto& constraint : constraintsToRemove)
    {
        cleaner.submitCommand(
                    new ClearConstraint(iscore::IDocument::path(constraint)));
    }

    for(auto& event : eventsToRemove)
    {
        cleaner.submitCommand(
                    new ClearEvent(iscore::IDocument::path(event)));
    }

    cleaner.commit();
}

void ScenarioGlobalCommandManager::deleteSelection(const ScenarioModel &scenario)
{
    // TODO quelques comportements bizarres à régler ...
    //*
    // 1. Select items
    auto constraintsToRemove = selectedElements(scenario.constraints());
    auto eventsToRemove = selectedElements(scenario.events());

    if(constraintsToRemove.size() != 0 || eventsToRemove.size() != 0)
    {
        MacroCommandDispatcher cleaner(new RemoveMultipleElements,
                                       m_commandStack,
                                       nullptr);

        // 2. Create a Delete command for each. For now : only emptying.
        for(auto& constraint : constraintsToRemove)
        {
            cleaner.submitCommand(
                        new RemoveConstraint(
                            iscore::IDocument::path(scenario),
                            constraint));
        }

        for(auto& event : eventsToRemove)
        {
            cleaner.submitCommand(
                        new RemoveEvent(
                            iscore::IDocument::path(scenario),
                            event));
        }

        // 3. Make a meta-command that binds them all and calls undo & redo on the queue.
        cleaner.commit();
    }
}
