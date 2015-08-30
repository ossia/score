#include "StandardCreationPolicy.hpp"
#include <Process/ScenarioModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>
#include <iscore/tools/NotifyingMap_impl.hpp>

void ScenarioCreate<TimeNodeModel>::undo(
        const Id<TimeNodeModel>& id,
        ScenarioModel& s)
{
    s.timeNodes.remove(id);
}

TimeNodeModel& ScenarioCreate<TimeNodeModel>::redo(
        const Id<TimeNodeModel>& id,
        const VerticalExtent& extent,
        const TimeValue& date,
        ScenarioModel& s)
{
    auto timeNode = new TimeNodeModel{id, extent, date, &s};
    s.timeNodes.add(timeNode);

    return *timeNode;
}

void ScenarioCreate<EventModel>::undo(
        const Id<EventModel>& id,
        ScenarioModel& s)
{
    auto& ev = s.event(id);
    s.timeNode(ev.timeNode()).removeEvent(id);
    s.events.remove(&ev);
}

EventModel& ScenarioCreate<EventModel>::redo(
        const Id<EventModel>& id,
        TimeNodeModel& timenode,
        const VerticalExtent& extent,
        ScenarioModel& s)
{
    auto ev = new EventModel{id, timenode.id(), extent, timenode.date(), &s};

    s.events.add(ev);
    timenode.addEvent(id);

    return *ev;
}


void ScenarioCreate<StateModel>::undo(
        const Id<StateModel> &id,
        ScenarioModel &s)
{
    auto& state = s.state(id);
    auto& ev = s.event(state.eventId());

    ev.removeState(id);

    s.states.remove(&state);
}

StateModel &ScenarioCreate<StateModel>::redo(
        const Id<StateModel> &id,
        EventModel &ev,
        double y,
        ScenarioModel &s)
{
    auto state = new StateModel{
            id,
            ev.id(),
            y,
            &s};

    ev.addState(state->id());

    s.states.add(state);

    return *state;
}

void ScenarioCreate<ConstraintModel>::undo(
        const Id<ConstraintModel>& id,
        ScenarioModel& s)
{
    auto& cst = s.constraint(id);

    auto& sev = s.state(cst.startState());
    auto& eev = s.state(cst.endState());

    sev.setNextConstraint(Id<ConstraintModel>{});
    eev.setPreviousConstraint(Id<ConstraintModel>{});

    s.constraints.remove(&cst);
}

ConstraintModel& ScenarioCreate<ConstraintModel>::redo(
        const Id<ConstraintModel>& id,
        const Id<ConstraintViewModel>& fullviewid,
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

    s.constraints.add(constraint);

    sst.setNextConstraint(id);
    est.setPreviousConstraint(id);

    const auto& sev = s.event(sst.eventId());
    const auto& eev = s.event(est.eventId());

    ConstraintDurations::Algorithms::changeAllDurations(*constraint,
                                                    eev.date() - sev.date());
    constraint->setStartDate(sev.date());

    return *constraint;
}
