// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioComponent.hpp"

namespace LocalTree
{

ScenarioComponentBase::ScenarioComponentBase(
    ossia::net::node_base& parent,
    Scenario::ProcessModel& scenario,
    const score::DocumentContext& sys,
    QObject* parent_obj)
    : ProcessComponent_T<
        Scenario::
            ProcessModel>{parent, scenario, sys, "ScenarioComponent", parent_obj}
    , m_intervalsNode{*node().create_child("intervals")}
    , m_eventsNode{*node().create_child("events")}
    , m_timeSyncsNode{*node().create_child("syncs")}
    , m_statesNode{*node().create_child("states")}
{
}

template <>
Interval* ScenarioComponentBase::make<Interval, Scenario::IntervalModel>(
    Scenario::IntervalModel& elt)
{
  return new Interval{m_intervalsNode,  elt, system(), this};
}

template <>
Event* ScenarioComponentBase::make<Event, Scenario::EventModel>(
    Scenario::EventModel& elt)
{
  return new Event{m_eventsNode, elt, system(), this};
}

template <>
TimeSync* ScenarioComponentBase::make<TimeSync, Scenario::TimeSyncModel>(
    Scenario::TimeSyncModel& elt)
{
  return new TimeSync{m_timeSyncsNode, elt, system(), this};
}

template <>
State* ScenarioComponentBase::make<State, Scenario::StateModel>(
    Scenario::StateModel& elt)
{
  return new State{m_statesNode, elt, system(), this};
}
}
