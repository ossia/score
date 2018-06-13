// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioComponent.hpp"



#include <Engine/LocalTree/Scenario/MetadataParameters.hpp>

namespace Engine
{
namespace LocalTree
{

ScenarioComponentBase::ScenarioComponentBase(
    const Id<score::Component>& id,
    ossia::net::node_base& parent,
    Scenario::ProcessModel& scenario,
    DocumentPlugin& sys,
    QObject* parent_obj)
    : ProcessComponent_T<Scenario::ProcessModel>{parent,
                                                 scenario,
                                                 sys,
                                                 id,
                                                 "ScenarioComponent",
                                                 parent_obj}
    , m_intervalsNode{*node().create_child("intervals")}
    , m_eventsNode{*node().create_child("events")}
    , m_timeSyncsNode{*node().create_child("syncs")}
    , m_statesNode{*node().create_child("states")}
{
}

template <>
Interval* ScenarioComponentBase::make<Interval, Scenario::IntervalModel>(
    const Id<score::Component>& id, Scenario::IntervalModel& elt)
{
  return new Interval{m_intervalsNode, id, elt, system(), this};
}

template <>
Event* ScenarioComponentBase::make<Event, Scenario::EventModel>(
    const Id<score::Component>& id, Scenario::EventModel& elt)
{
  return new Event{m_eventsNode, id, elt, system(), this};
}

template <>
TimeSync* ScenarioComponentBase::make<TimeSync, Scenario::TimeSyncModel>(
    const Id<score::Component>& id, Scenario::TimeSyncModel& elt)
{
  return new TimeSync{m_timeSyncsNode, id, elt, system(), this};
}

template <>
State* ScenarioComponentBase::make<State, Scenario::StateModel>(
    const Id<score::Component>& id, Scenario::StateModel& elt)
{
  return new State{m_statesNode, id, elt, system(), this};
}
}
}
