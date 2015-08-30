#include "StandardRemovalPolicy.hpp"
#include <Process/ScenarioModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>
#include "StandardCreationPolicy.hpp"

static void removeEventFromTimeNode(
        ScenarioModel& scenario,
        const Id<EventModel>& eventId)
{
    // We have to make a copy else the iterator explodes.
    auto timenodes = scenario.timeNodes.map();
    for(auto& timeNode : timenodes)
    {
        if(timeNode.removeEvent(eventId))
        {
            if(timeNode.events().isEmpty())
            {
                // TODO transform this into a class with algorithms on timenodes + scenario, etc.
                // Note : this changes the scenario.timeNodes() iterator, however
                // since we return afterwards there is no problem.
                ScenarioCreate<TimeNodeModel>::undo(timeNode.id(), scenario);
            }
        }
    }
}



void StandardRemovalPolicy::removeConstraint(
        ScenarioModel& scenario,
        const Id<ConstraintModel>& constraintId)
{
    auto cstr_it = scenario.constraints.find(constraintId);
    if(cstr_it != scenario.constraints.end())
    {
        ConstraintModel& cstr =  *cstr_it;
        auto& sst = scenario.state(cstr.startState());
        sst.setNextConstraint(Id<ConstraintModel>{});

        auto& est = scenario.state(cstr.endState());
        est.setPreviousConstraint(Id<ConstraintModel>{});

        scenario.constraints.remove(&cstr);
    }
    else
    {
        qDebug() << Q_FUNC_INFO << "Warning : removing a non-existant constraint";
    }
}


void StandardRemovalPolicy::removeState(
        ScenarioModel& scenario,
        const Id<StateModel>& stateId)
{
    auto& state = scenario.state(stateId);
    if(state.previousConstraint())
    {
        StandardRemovalPolicy::removeConstraint(scenario, state.previousConstraint());
    }

    if(state.nextConstraint()){
        StandardRemovalPolicy::removeConstraint(scenario, state.nextConstraint());
    }

    scenario.states.remove(&state);

}

void StandardRemovalPolicy::removeEventStatesAndConstraints(
        ScenarioModel& scenario,
        const Id<EventModel>& eventId)
{
    auto& ev = scenario.event(eventId);

    for(const auto& state : ev.states())
    {
        StandardRemovalPolicy::removeState(scenario, state);
    }

    removeEventFromTimeNode(scenario, eventId);

    scenario.events.remove(&ev);
}
