#pragma once
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>

// Constraints
template<typename Scenario_T>
const auto& startState(
        const ConstraintModel& cst,
        const Scenario_T& scenario)
{
    return scenario.state(cst.startState());
}

template<typename Scenario_T>
const auto& endState(
        const ConstraintModel& cst,
        const Scenario_T& scenario)
{
    return scenario.state(cst.endState());
}

template<typename Scenario_T>
const auto& startEvent(
        const ConstraintModel& cst,
        const Scenario_T& scenario)
{
    return scenario.event(startState(cst, scenario).eventId());
}

template<typename Scenario_T>
const auto& endEvent(
        const ConstraintModel& cst,
        const Scenario_T& scenario)
{
    return scenario.event(endState(cst, scenario).eventId());
}


template<typename Scenario_T>
const auto& startTimeNode(
        const ConstraintModel& cst,
        const Scenario_T& scenario)
{
    return scenario.timeNode(startEvent(cst, scenario).timeNode());
}

template<typename Scenario_T>
const auto& endTimeNode(
        const ConstraintModel& cst,
        const Scenario_T& scenario)
{
    return scenario.timeNode(endEvent(cst, scenario).timeNode());
}

// Events
template<typename Scenario_T>
const auto& parentTimeNode(
        const EventModel& ev,
        const Scenario_T& scenario)
{
    return scenario.timeNode(ev.timeNode());
}

// States
template<typename Scenario_T>
const auto& parentEvent(
        const StateModel& st,
        const Scenario_T& scenario)
{
    return scenario.event(st.eventId());
}


template<typename Scenario_T>
const auto& previousConstraint(
        const StateModel& st,
        const Scenario_T& scenario)
{
    return scenario.constraint(st.previousConstraint());
}

template<typename Scenario_T>
const auto& nextConstraint(
        const StateModel& st,
        const Scenario_T& scenario)
{
    return scenario.constraint(st.nextConstraint());
}


template<typename Scenario_T>
auto nextConstraints(
        const EventModel& ev,
        const Scenario_T& scenario)
{
    std::list<Id<ConstraintModel>> constraints;
    for(const Id<StateModel>& state : ev.states())
    {
        const StateModel& st = scenario.states.at(state);
        if(const auto& cst_id = st.nextConstraint())
            constraints.push_back(cst_id);
    }
    return constraints;
}
template<typename Scenario_T>
auto previousConstraints(
        const EventModel& ev,
        const Scenario_T& scenario)
{
    std::list<Id<ConstraintModel>> constraints;
    for(const Id<StateModel>& state : ev.states())
    {
        const StateModel& st = scenario.state(state);
        if(const auto& cst_id = st.previousConstraint())
            constraints.push_back(cst_id);
    }
    return constraints;
}

template<typename Scenario_T>
auto nextConstraints(
        const TimeNodeModel& tn,
        const Scenario_T& scenario)
{
    std::list<Id<ConstraintModel>> constraints;
    for(const Id<EventModel>& event_id : tn.events())
    {
        const EventModel& event = scenario.events.at(event_id);
        auto prev = nextConstraints(event, scenario);
        constraints.splice(constraints.end(), prev);
    }

    return constraints;
}


template<typename Scenario_T>
auto previousConstraints(
        const TimeNodeModel& tn,
        const Scenario_T& scenario)
{
    std::list<Id<ConstraintModel>> constraints;
    for(const Id<EventModel>& event_id : tn.events())
    {
        const EventModel& event = scenario.event(event_id);
        auto prev = previousConstraints(event, scenario);
        constraints.splice(constraints.end(), prev);
    }

    return constraints;
}


