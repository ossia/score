#include "InterpolateStates.hpp"

#include <Commands/CreateCurveFromStates.hpp>
#include <Commands/InterpolateMacro.hpp>

#include <Automation/AutomationModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Process/ScenarioModel.hpp>

#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>

#include <core/document/Document.hpp>

void InterpolateStates(iscore::Document* doc)
{
    using namespace std;
    // Fetch the selected constraints
    auto sel = doc->
            selectionStack().
            currentSelection();

    QList<const ConstraintModel*> selected_constraints;
    for(auto obj : sel)
    {
        // TODO replace with a virtual Element::type() which will be faster.
        if(auto cst = dynamic_cast<const ConstraintModel*>(obj.data()))
        {
            if(cst->selection.get() && dynamic_cast<ScenarioModel*>(cst->parent()))
            {
                selected_constraints.push_back(cst);
            }
        }
    }

    // For each constraint, interpolate between the states in its start event and end event.

    // TODO maybe template it instead?
    MacroCommandDispatcher macro{new InterpolateMacro,
                doc->commandStack()};
    // They should all be in the same scenario so we can select the first.
    ScenarioModel* scenar =
            selected_constraints.empty()
            ? nullptr
            : dynamic_cast<ScenarioModel*>(selected_constraints.first()->parent());

    for(auto& constraint : selected_constraints)
    {
        const auto& startState = scenar->state(constraint->startState());
        const auto& endState = scenar->state(constraint->endState());

        iscore::MessageList startMessages = startState.messages().flatten();
        iscore::MessageList endMessages = endState.messages().flatten();

        for(auto& message : startMessages)
        {
            if(!message.value.val.isNumeric())
                continue;

            auto it = std::find_if(begin(endMessages),
                                   end(endMessages),
                                   [&] (const iscore::Message& arg) {
                return message.address == arg.address
                        && arg.value.val.isNumeric()
                        && message.value.val.impl().which() == arg.value.val.impl().which()
                        && message.value != arg.value; });

            if(it != end(endMessages))
            {
                auto has_existing_curve = std::find_if(
                            constraint->processes.begin(),
                            constraint->processes.end(),
                            [&] (const Process& proc) {
                    auto ptr = dynamic_cast<const AutomationModel*>(&proc);
                    if(ptr && ptr->address() == message.address)
                        return true;
                    return false;
                });

                if(has_existing_curve != constraint->processes.end())
                    continue;

                auto cmd = new CreateCurveFromStates{
                        *constraint,
                        message.address,
                        iscore::convert::value<double>(message.value),
                        iscore::convert::value<double>((*it).value)};
                macro.submitCommand(cmd);
            }
        }
    }

    macro.commit();
}
