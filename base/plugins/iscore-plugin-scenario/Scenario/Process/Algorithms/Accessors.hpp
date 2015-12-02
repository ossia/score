#pragma once
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
/*
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
*/

template<typename Scenario_T>
const auto& startEvent(
        const ConstraintModel& cst,
        const Scenario_T& scenario)
{
    return scenario.event(scenario.state(cst.startState()).eventId());
}

template<typename Scenario_T>
const auto& endEvent(
        const ConstraintModel& cst,
        const Scenario_T& scenario)
{
    return scenario.event(scenario.state(cst.endState()).eventId());
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
