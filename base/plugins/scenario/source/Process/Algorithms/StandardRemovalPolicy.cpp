#include "StandardRemovalPolicy.hpp"
#include <Process/ScenarioModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>


void removeEventFromTimeNode(ScenarioModel& scenario,
                             id_type<EventModel> eventId)
{
    for(auto& timeNode : scenario.timeNodes())
    {
        if(timeNode->removeEvent(eventId))
        {
            if(timeNode->isEmpty())
            {
                StandardRemovalPolicy::removeTimeNode(scenario,
                                                      timeNode->id());
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

void StandardRemovalPolicy::removeEvent(ScenarioModel& scenario,
                                        id_type<EventModel> eventId)
{
    auto ev = scenario.event(eventId);

    auto constraints = ev->previousConstraints();
    for(auto constraint : constraints)
    {
        StandardRemovalPolicy::removeConstraint(scenario, constraint);
    }

    constraints = ev->nextConstraints();
    for(auto constraint : ev->nextConstraints())
    {
        StandardRemovalPolicy::removeConstraint(scenario, constraint);
    }

    removeEventFromTimeNode(scenario, eventId);

    scenario.removeEvent(ev);
}

void StandardRemovalPolicy::removeTimeNode(ScenarioModel& scenario,
                                           id_type<TimeNodeModel> timeNodeId)
{
    scenario.removeTimeNode(scenario.timeNode(timeNodeId));
}
