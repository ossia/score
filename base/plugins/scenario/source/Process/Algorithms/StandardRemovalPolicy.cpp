#include "StandardRemovalPolicy.hpp"
#include <Process/ScenarioModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>
#include "StandardCreationPolicy.hpp"

void removeEventFromTimeNode(ScenarioModel& scenario,
                             id_type<EventModel> eventId)
{
    for(auto& timeNode : scenario.timeNodes())
    {
        if(timeNode->removeEvent(eventId))
        {
            if(timeNode->isEmpty())
            {
                // TODO transform this into a class with algorithms on timenodes + scenario, etc.
                CreateTimeNodeMin::undo(timeNode->id(), scenario);
            }

            return;
        }
    }
}


void StandardRemovalPolicy::removeConstraint(ScenarioModel& scenario,
                                             id_type<ConstraintModel> constraintId)
{
    auto cstr = scenario.constraint(constraintId);
    auto sev = scenario.event(cstr->startEvent());
    sev->removeNextConstraint(constraintId);

    auto eev = scenario.event(cstr->endEvent());
    eev->removePreviousConstraint(constraintId);

    scenario.removeConstraint(cstr);
}

void StandardRemovalPolicy::removeEventAndConstraints(ScenarioModel& scenario,
                                                      id_type<EventModel> eventId)
{
    auto ev = scenario.event(eventId);

    for(auto constraint : ev->constraints())
    {
        StandardRemovalPolicy::removeConstraint(scenario, constraint);
    }

    removeEventFromTimeNode(scenario, eventId);

    scenario.removeEvent(ev);
}
