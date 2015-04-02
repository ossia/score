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
    /*
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

    constraint->setStartDate(scenario.event(startEventId)->date());
    constraint->setDefaultDuration(constraint_duration);
    constraint->setMinDuration(constraint_duration);
    constraint->setMaxDuration(constraint_duration);
    event->setDate(constraint->startDate() + constraint->defaultDuration());

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
    */
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

    // Error checking if it did not go well ? Rollback ?
    // Else...
    constraint->setStartEvent(sev->id());
    constraint->setEndEvent(eev->id());

    constraint->setStartDate(sev->date());
    auto constraint_duration = eev->date() - sev->date();
    constraint->setDefaultDuration(constraint_duration);
    constraint->setMinDuration(constraint_duration);
    constraint->setMaxDuration(constraint_duration);
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
                    newEvent->heightPercentage(),
                    &scenario};
    timeNode->addEvent(eventId);

    scenario.addTimeNode(timeNode);
    return timeNode;
}


void CreateTimeNodeMin::undo(id_type<TimeNodeModel> id,
                             ScenarioModel& s)
{
    s.removeTimeNode(s.timeNode(id));
}

TimeNodeModel& CreateTimeNodeMin::redo(id_type<TimeNodeModel> id,
                                       const TimeValue& date,
                                       double y,
                                       ScenarioModel& s)
{
    auto timeNode = new TimeNodeModel {id, date, y, &s};
    s.addTimeNode(timeNode);
    return *timeNode;
}

void CreateEventMin::undo(id_type<EventModel> id,
                          ScenarioModel& s)
{
    auto ev = s.event(id);
    s.timeNode(ev->timeNode())->removeEvent(id);
    s.removeEvent(ev);
}

EventModel& CreateEventMin::redo(id_type<EventModel> id,
                                 TimeNodeModel& timenode,
                                 double y,
                                 ScenarioModel& s)
{
    auto ev = new EventModel{id, timenode.id(), y, &s};
    ev->setDate(timenode.date());

    s.addEvent(ev);
    timenode.addEvent(id);

    return *ev;
}



void CreateConstraintMin::undo(id_type<ConstraintModel> id,
                               ScenarioModel& s)
{
    auto cst = s.constraint(id);
    auto sev = s.event(cst->startEvent());
    auto eev = s.event(cst->endEvent());
    sev->removeNextConstraint(id);
    eev->removePreviousConstraint(id);

    s.removeConstraint(cst);

}

ConstraintModel&CreateConstraintMin::redo(id_type<ConstraintModel> id,
                                          id_type<AbstractConstraintViewModel> fullviewid,
                                          EventModel& sev,
                                          EventModel& eev,
                                          double ypos,
                                          ScenarioModel& s)
{
    auto constraint = new ConstraintModel {
                      id,
                      fullviewid,
                      ypos,
                      &s};

    sev.addNextConstraint(id);
    eev.addPreviousConstraint(id);

    constraint->setStartEvent(sev.id());
    constraint->setEndEvent(eev.id());

    auto dur = eev.date() - sev.date();
    constraint->setDefaultDuration(dur);
    constraint->setMinDuration(dur);
    constraint->setMaxDuration(dur);
    constraint->setStartDate(sev.date());

    s.addConstraint(constraint);
    return *constraint;
}
