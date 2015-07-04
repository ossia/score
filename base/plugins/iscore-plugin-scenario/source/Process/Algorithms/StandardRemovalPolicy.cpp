#include "StandardRemovalPolicy.hpp"
#include <Process/ScenarioModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>
#include "StandardCreationPolicy.hpp"

void removeEventFromTimeNode(
        ScenarioModel& scenario,
        const id_type<EventModel>& eventId)
{
    for(TimeNodeModel* timeNode : scenario.timeNodes())
    {
        if(timeNode->removeEvent(eventId))
        {
            if(timeNode->events().isEmpty())
            {
                // TODO transform this into a class with algorithms on timenodes + scenario, etc.
                // Note : this changes the scenario.timeNodes() iterator, however
                // since we return afterwards there is no problem.
                ScenarioCreate<TimeNodeModel>::undo(timeNode->id(), scenario);
            }

            return;
        }
    }
}


void StandardRemovalPolicy::removeConstraint(
        ScenarioModel& scenario,
        const id_type<ConstraintModel>& constraintId)
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
    /*
    auto& cstr = scenario.constraint(constraintId);
    auto& sev = scenario.event(cstr.startEvent());
    sev.removeNextConstraint(constraintId);

    auto& eev = scenario.event(cstr.endEvent());
    eev.removePreviousConstraint(constraintId);

    scenario.removeConstraint(&cstr);
    */
}

void StandardRemovalPolicy::removeEventAndConstraints(
        ScenarioModel& scenario,
        const id_type<EventModel>& eventId)
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
    /*
    auto& ev = scenario.event(eventId);

    for(const auto& constraint : ev.constraints())
    {
        StandardRemovalPolicy::removeConstraint(scenario, constraint);
    }

    removeEventFromTimeNode(scenario, eventId);

    scenario.removeEvent(&ev);
    */
}
