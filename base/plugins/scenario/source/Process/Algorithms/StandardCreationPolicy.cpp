#include "StandardCreationPolicy.hpp"
#include <Process/ScenarioModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>

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

    ConstraintModel::Algorithms::changeAllDurations(*constraint,
                                                    eev.date() - sev.date());
    constraint->setStartDate(sev.date());

    s.addConstraint(constraint);
    return *constraint;
}
