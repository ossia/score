#include "StandardCreationPolicy.hpp"
#include <Process/ScenarioModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>

void StandardCreationPolicy::createConstraintAndEndEventFromEvent(
        ScenarioModel& scenario,
        id_type<EventModel> startEventId,
        TimeValue constraint_duration,
        double heightPos,
        id_type<ConstraintModel> newConstraintId,
        id_type<AbstractConstraintViewModel> newConstraintFullViewId,
        id_type<EventModel> newEventId)
{
    auto startEvent = scenario.event(startEventId);

    auto constraint = new ConstraintModel {newConstraintId,
                      newConstraintFullViewId,
                      scenario.event(startEventId)->heightPercentage(),
                      &scenario};
    auto event = new EventModel{newEventId,
                 heightPos,
                 &scenario};


    if(startEventId == scenario.startEvent()->id())
    {
        constraint->setHeightPercentage(heightPos);
    }
    else
    {
        constraint->setHeightPercentage((heightPos + startEvent->heightPercentage()) / 2);
    }

    // TEMPORARY :
    constraint->setStartDate(scenario.event(startEventId)->date());
    constraint->setDefaultDuration(constraint_duration);
    event->setDate(constraint->startDate() + constraint->defaultDuration());

    //	auto ossia_tn0 = this->event(startEventId)->apiObject();
    //	auto ossia_tn1 = event->apiObject();
    //	auto ossia_tb = constraint->apiObject();

    //	m_scenario->addTimeBox(*ossia_tb,
    //						   *ossia_tn0,
    //						   *ossia_tn1);

    // Error checking if it did not go well ? Rollback ?
    // Else...
    constraint->setStartEvent(startEventId);
    constraint->setEndEvent(event->id());

    // From now on everything must be in a valid state.
    scenario.addEvent(event);
    scenario.addConstraint(constraint);

    // link constraint with event
    event->addPreviousConstraint(newConstraintId);
    startEvent->addNextConstraint(newConstraintId);
}


void StandardCreationPolicy::createConstraintBetweenEvents(
        ScenarioModel& scenario,
        id_type<EventModel> startEventId,
        id_type<EventModel> endEventId,
        id_type<ConstraintModel> newConstraintModelId,
        id_type<AbstractConstraintViewModel> newConstraintFullViewId)
{
    auto sev = scenario.event(startEventId);
    auto eev = scenario.event(endEventId);
    auto constraint = new ConstraintModel {newConstraintModelId,
                      newConstraintFullViewId,
                      &scenario};

    /*	auto ossia_tn0 = sev->apiObject();
    auto ossia_tn1 = eev->apiObject();
    auto ossia_tb = inter->apiObject();

    m_scenario->addTimeBox(*ossia_tb,
                           *ossia_tn0,
                           *ossia_tn1);
    */
    // Error checking if it did not go well ? Rollback ?
    // Else...
    constraint->setStartEvent(sev->id());
    constraint->setEndEvent(eev->id());

    constraint->setStartDate(sev->date());
    constraint->setDefaultDuration(eev->date() - sev->date());
    constraint->setHeightPercentage((sev->heightPercentage() + eev->heightPercentage()) / 2.);

    sev->addNextConstraint(newConstraintModelId);
    eev->addPreviousConstraint(newConstraintModelId);

    // From now on everything must be in a valid state.
    scenario.addConstraint(constraint);
}


TimeNodeModel* StandardCreationPolicy::createTimeNode(ScenarioModel& scenario,
                                                 id_type<TimeNodeModel> timeNodeId,
                                                 id_type<EventModel> eventId)
{
    auto newEvent = scenario.event(eventId);

    auto timeNode = new TimeNodeModel {timeNodeId,
                    newEvent->date(),
                    &scenario};
    timeNode->addEvent(eventId);
    timeNode->setY(newEvent->heightPercentage());

    scenario.addTimeNode(timeNode);
    return timeNode;
}
