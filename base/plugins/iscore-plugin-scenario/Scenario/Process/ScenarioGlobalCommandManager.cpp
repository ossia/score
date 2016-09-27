#include <Scenario/Commands/ClearSelection.hpp>
#include <Scenario/Commands/Scenario/Deletions/ClearConstraint.hpp>
#include <Scenario/Commands/Scenario/Deletions/ClearState.hpp>
#include <Scenario/Commands/Scenario/Deletions/RemoveSelection.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <QDebug>
#include <algorithm>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include "ScenarioGlobalCommandManager.hpp"
#include <iscore/selection/Selection.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/utilsCPP11.hpp>

#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Commands/Scenario/Merge/MergeTimeNodes.hpp>
namespace iscore {
class CommandStackFacade;
}  // namespace iscore

using namespace Scenario::Command;
using namespace iscore::IDocument; // for ::path
void Scenario::clearContentFromSelection(
        const Scenario::ProcessModel& scenario,
        const iscore::CommandStackFacade& stack)
{
    // 1. Select items
    auto constraintsToRemove = selectedElements(scenario.constraints);
    auto statesToRemove = selectedElements(scenario.states);

    MacroCommandDispatcher<ClearSelection> cleaner{stack};

    // 2. Create a Clear command for each.
    for(auto& constraint : constraintsToRemove)
    {
        cleaner.submitCommand(new ClearConstraint(*constraint));
    }

    for(auto& state : statesToRemove)
    {
        cleaner.submitCommand(new ClearState(path(*state)));
    }

    cleaner.commit();
}


void Scenario::removeSelection(
        const Scenario::ProcessModel& scenario,
        const iscore::CommandStackFacade& stack)
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
        const iscore::CommandStackFacade&)
{
    // Shall do nothing
}

void Scenario::clearContentFromSelection(
        const BaseScenario&,
        const iscore::CommandStackFacade&)
{
    ISCORE_TODO;
}

template<typename T>
struct DateComparator
{
        const Scenario::ProcessModel* scenario;
        bool operator()(const T* lhs, const T* rhs)
        {
            return Scenario::date(*lhs, *scenario) < Scenario::date(*rhs, *scenario);
        }
};

template<typename T>
auto make_ordered(const Scenario::ProcessModel& scenario)
{
    using comp_t = DateComparator<T>;
    using set_t = std::set<const T*, comp_t>;

    set_t the_set(comp_t{&scenario});

    auto cont = Scenario::ScenarioElementTraits<Scenario::ProcessModel, T>::accessor;
    for(auto& tn : selectedElements(cont(scenario)))
    {
        the_set.insert(tn);
    }
    return the_set;
}

void Scenario::mergeTimeNodes(
        const Scenario::ProcessModel & scenario,
        const iscore::CommandStackFacade & f)
{
    // We merge all the furthest timenodes to the first one.
    auto timenodes = make_ordered<TimeNodeModel>(scenario);
    auto states = make_ordered<StateModel>(scenario);
    auto events = make_ordered<EventModel>(scenario);

    if(timenodes.size() < 2)
    {
        if(states.size() == 2)
        {
            auto it = states.begin();
            auto& first = Scenario::parentTimeNode(**it, scenario);
            auto& second = Scenario::parentTimeNode(**(++it), scenario);

            auto cmd = new Command::MergeTimeNodes<ProcessModel>(scenario, second.id(), first.id());
            f.redoAndPush(cmd);
        }
    }
    else
    {
        auto it = timenodes.begin();
        auto first_tn = (*it)->id();
        for(++it; it != timenodes.end(); ++it)
        {
            auto cmd = new Command::MergeTimeNodes<ProcessModel>(scenario, first_tn, (*it)->id());
            f.redoAndPush(cmd);
        }
    }
}
