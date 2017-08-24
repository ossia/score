// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LoopComponent.hpp"
#include <ossia/editor/state/state_element.hpp>
#include <Engine/LocalTree/Scenario/MetadataParameters.hpp>

namespace Engine
{
namespace LocalTree
{

LoopComponentBase::LoopComponentBase(
    const Id<iscore::Component>& id,
    ossia::net::node_base& parent,
    Loop::ProcessModel& loop,
    DocumentPlugin& sys,
    QObject* parent_obj)
    : ProcessComponent_T<Loop::ProcessModel>{parent,
                                             loop,
                                             sys,
                                             id,
                                             "LoopComponent",
                                             parent_obj}
    , m_constraintsNode{*node().create_child("constraints")}
    , m_eventsNode{*node().create_child("events")}
    , m_timeSyncsNode{*node().create_child("timesyncs")}
    , m_statesNode{*node().create_child("states")}
{
}

template <>
Constraint* LoopComponentBase::make<Constraint, Scenario::ConstraintModel>(
    const Id<iscore::Component>& id, Scenario::ConstraintModel& elt)
{
  return new Constraint{m_constraintsNode, id, elt, system(), this};
}

template <>
Event* LoopComponentBase::make<Event, Scenario::EventModel>(
    const Id<iscore::Component>& id, Scenario::EventModel& elt)
{
  return new Event{m_eventsNode, id, elt, system(), this};
}

template <>
TimeSync* LoopComponentBase::make<TimeSync, Scenario::TimeSyncModel>(
    const Id<iscore::Component>& id, Scenario::TimeSyncModel& elt)
{
  return new TimeSync{m_timeSyncsNode, id, elt, system(), this};
}

template <>
State* LoopComponentBase::make<State, Scenario::StateModel>(
    const Id<iscore::Component>& id, Scenario::StateModel& elt)
{
  return new State{m_statesNode, id, elt, system(), this};
}
}
}
