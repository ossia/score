#pragma once
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>

// Constraints
namespace Scenario
{
template<typename Scenario_T>
StateModel& startState(
        const ConstraintModel& cst,
        const Scenario_T& scenario)
{
    return scenario.state(cst.startState());
}

template<typename Scenario_T>
StateModel& endState(
        const ConstraintModel& cst,
        const Scenario_T& scenario)
{
    return scenario.state(cst.endState());
}

template<typename Scenario_T>
const EventModel& startEvent(
        const ConstraintModel& cst,
        const Scenario_T& scenario)
{
    return scenario.event(startState(cst, scenario).eventId());
}

template<typename Scenario_T>
const EventModel& endEvent(
        const ConstraintModel& cst,
        const Scenario_T& scenario)
{
    return scenario.event(endState(cst, scenario).eventId());
}


template<typename Scenario_T>
const TimeNodeModel& startTimeNode(
        const ConstraintModel& cst,
        const Scenario_T& scenario)
{
    return scenario.timeNode(startEvent(cst, scenario).timeNode());
}

template<typename Scenario_T>
const TimeNodeModel& endTimeNode(
        const ConstraintModel& cst,
        const Scenario_T& scenario)
{
    return scenario.timeNode(endEvent(cst, scenario).timeNode());
}

// Events
template<typename Scenario_T>
const TimeNodeModel& parentTimeNode(
        const EventModel& ev,
        const Scenario_T& scenario)
{
    return scenario.timeNode(ev.timeNode());
}


// States
template<typename Scenario_T>
const EventModel& parentEvent(
        const StateModel& st,
        const Scenario_T& scenario)
{
    return scenario.event(st.eventId());
}

template<typename Scenario_T>
const TimeNodeModel& parentTimeNode(
        const StateModel& st,
        const Scenario_T& scenario)
{
    return parentTimeNode(parentEvent(st, scenario), scenario);
}

// This one is just here to allow generic facilities
template<typename Scenario_T>
const TimeNodeModel& parentTimeNode(
        const TimeNodeModel& st,
        const Scenario_T& )
{
    return st;
}


template<typename Scenario_T>
const ConstraintModel& previousConstraint(
        const StateModel& st,
        const Scenario_T& scenario)
{
    ISCORE_ASSERT(st.previousConstraint());
    return scenario.constraint(*st.previousConstraint());
}

template<typename Scenario_T>
const ConstraintModel& nextConstraint(
        const StateModel& st,
        const Scenario_T& scenario)
{
    ISCORE_ASSERT(st.nextConstraint());
    return scenario.constraint(*st.nextConstraint());
}


template<typename Scenario_T>
auto nextConstraints(
        const EventModel& ev,
        const Scenario_T& scenario)
{
    std::list<Id<ConstraintModel>> constraints;
    for(const Id<StateModel>& state : ev.states())
    {
        const StateModel& st = scenario.state(state);
        if(const auto& cst_id = st.nextConstraint())
            constraints.push_back(*cst_id);
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
            constraints.push_back(*cst_id);
    }
    return constraints;
}

// TimeNodes
template<typename Scenario_T>
auto nextConstraints(
        const TimeNodeModel& tn,
        const Scenario_T& scenario)
{
    std::list<Id<ConstraintModel>> constraints;
    for(const Id<EventModel>& event_id : tn.events())
    {
        const EventModel& event = scenario.event(event_id);
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

template<typename Scenario_T>
auto states(
        const TimeNodeModel& tn,
        const Scenario_T& scenario)
{
    std::list<Id<StateModel>> stateList;
    for(const Id<EventModel>& event_id : tn.events())
    {
        const EventModel& event = scenario.event(event_id);
        auto st = event.states().toList().toStdList();
        stateList.splice(stateList.end(), st);
    }

    return stateList;
}

// Dates
template<typename Element_T, typename Scenario_T>
const auto& date(
        const Element_T& e,
        const Scenario_T& scenario)
{
    return parentTimeNode(e, scenario).date();
}

template<typename Element_T>
Scenario::ScenarioInterface& parentScenario(Element_T&& e)
{
    return *safe_cast<Scenario::ScenarioInterface*>(e.parent());
}
}
