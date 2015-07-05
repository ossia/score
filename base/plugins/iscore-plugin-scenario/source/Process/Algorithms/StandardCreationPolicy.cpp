#include "StandardCreationPolicy.hpp"
#include <Process/ScenarioModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>

void ScenarioCreate<TimeNodeModel>::undo(
        const id_type<TimeNodeModel>& id,
        ScenarioModel& s)
{
    s.removeTimeNode(&s.timeNode(id));
}

TimeNodeModel& ScenarioCreate<TimeNodeModel>::redo(
        const id_type<TimeNodeModel>& id,
        const VerticalExtent& extent,
        const TimeValue& date,
        ScenarioModel& s)
{
    auto timeNode = new TimeNodeModel{id, extent, date, &s};
    s.addTimeNode(timeNode);

    return *timeNode;

}

void ScenarioCreate<EventModel>::undo(
        const id_type<EventModel>& id,
        ScenarioModel& s)
{
    auto& ev = s.event(id);
    s.timeNode(ev.timeNode()).removeEvent(id);
    s.removeEvent(&ev);
}

EventModel& ScenarioCreate<EventModel>::redo(
        const id_type<EventModel>& id,
        TimeNodeModel& timenode,
        const VerticalExtent& extent,
        ScenarioModel& s)
{
    auto ev = new EventModel{id, timenode.id(), extent, timenode.date(), &s};

    s.addEvent(ev);
    timenode.addEvent(id);

    return *ev;
}


void ScenarioCreate<StateModel>::undo(
        const id_type<StateModel> &id,
        ScenarioModel &s)
{
    auto& state = s.displayedState(id);
    auto& ev = s.event(state.eventId());

    ev.removeDisplayedState(id);

    s.removeDisplayedState(&state);
}

StateModel &ScenarioCreate<StateModel>::redo(
        const id_type<StateModel> &id,
        EventModel &ev,
        double y,
        ScenarioModel &s)
{
    auto state = new StateModel{
            id,
            ev.id(),
            y,
            &s};

    ev.addDisplayedState(state->id());

    s.addDisplayedState(state);

    return *state;
}

//TODO unused ?
void ScenarioCreate<ConstraintModel>::undo(
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

ConstraintModel& ScenarioCreate<ConstraintModel>::redo(
        const id_type<ConstraintModel>& id,
        const id_type<AbstractConstraintViewModel>& fullviewid,
        StateModel& sst,
        StateModel& est,
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


