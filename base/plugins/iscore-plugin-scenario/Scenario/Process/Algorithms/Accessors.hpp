#pragma once
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
/*
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
*/
template<typename Scenario_T>
const TimeNodeModel& startTimeNode(
        const ConstraintModel& cst,
        const Scenario_T& scenario)
{
    return scenario.timeNode(
                scenario.event(
                    scenario.state(cst.startState()).eventId()).timeNode());

}

template<typename Scenario_T>
const TimeNodeModel& endTimeNode(
        const ConstraintModel& cst,
        const Scenario_T& scenario)
{
    return scenario.timeNode(
                scenario.event(
                    scenario.state(cst.endState()).eventId()).timeNode());
}
