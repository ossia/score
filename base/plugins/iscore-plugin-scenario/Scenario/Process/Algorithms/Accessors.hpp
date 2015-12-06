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
