#include "StandardCreationPolicy.hpp"
#include <Process/ScenarioModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>

void CreateTimeNodeMin::undo(
        const id_type<TimeNodeModel>& id,
        ScenarioModel& s)
{
    s.removeTimeNode(&s.timeNode(id));
}

TimeNodeModel& CreateTimeNodeMin::redo(
        const id_type<TimeNodeModel>& id,
        const TimeValue& date,
        ScenarioModel& s)
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
    /*
    auto timeNode = new TimeNodeModel{id, date, &s};
    s.addTimeNode(timeNode);
    return *timeNode;
    */
}

void CreateEventMin::undo(
        const id_type<EventModel>& id,
        ScenarioModel& s)
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
    /*
    auto& ev = s.event(id);
    s.timeNode(ev.timeNode()).removeEvent(id);
    s.removeEvent(&ev);
    */
}

EventModel& CreateEventMin::redo(
        const id_type<EventModel>& id,
        TimeNodeModel& timenode,
        ScenarioModel& s)
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
    /*
    auto ev = new EventModel{id, timenode.id(), &s};
    ev->setDate(timenode.date());

    s.addEvent(ev);
    timenode.addEvent(id);

    return *ev;
*/
}


void CreateStateMin::undo(
        const id_type<DisplayedStateModel> &id,
        ScenarioModel &s)
{
    auto& state = s.displayedState(id);
    auto& ev = s.event(state.eventId());

    ev.removeDisplayedState(id);

    s.removeDisplayedState(&state);
}

DisplayedStateModel &CreateStateMin::redo(
        const id_type<DisplayedStateModel> &id,
        EventModel &ev,
        double y,
        ScenarioModel &s)
{
    auto state = new DisplayedStateModel{
            id,
            ev.id(),
            y,
            &s};

    ev.addDisplayedState(state->id());

    s.addDisplayedState(state);
}

//TODO unused ?
void CreateConstraintMin::undo(
        const id_type<ConstraintModel>& id,
        ScenarioModel& s)
{
    auto& cst = s.constraint(id);

    auto& sev = s.displayedState(cst.startState());
    auto& eev = s.displayedState(cst.endState());

    sev.setNextConstraint(id_type<ConstraintModel>{});
    eev.setPreviousConstraint(id_type<ConstraintModel>{});

    s.removeConstraint(&cst);
}

ConstraintModel& CreateConstraintMin::redo(
        const id_type<ConstraintModel>& id,
        const id_type<AbstractConstraintViewModel>& fullviewid,
        DisplayedStateModel& sst,
        DisplayedStateModel& est,
        double ypos,
        ScenarioModel& s)
{
    auto constraint = new ConstraintModel {
                      id,
                      fullviewid,
                      ypos,
                      &s};

    constraint->setStartState(sst.id());
    constraint->setEndState(est.id());

    s.addConstraint(constraint);

    sst.setNextConstraint(id);
    est.setPreviousConstraint(id);

    const auto& sev = s.event(sst.eventId());
    const auto& eev = s.event(est.eventId());

    ConstraintModel::Algorithms::changeAllDurations(*constraint,
                                                    eev.date() - sev.date());
    constraint->setStartDate(sev.date());

    return *constraint;
}


