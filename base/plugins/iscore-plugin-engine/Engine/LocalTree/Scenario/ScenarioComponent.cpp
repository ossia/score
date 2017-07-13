// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioComponent.hpp"
#include <ossia/editor/state/state_element.hpp>
#include <Engine/LocalTree/Scenario/MetadataParameters.hpp>

namespace Engine
{
namespace LocalTree
{

ScenarioComponentBase::ScenarioComponentBase(
    const Id<iscore::Component>& id,
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
    , m_constraintsNode{*node().create_child("constraints")}
    , m_eventsNode{*node().create_child("events")}
    , m_timeNodesNode{*node().create_child("timenodes")}
    , m_statesNode{*node().create_child("states")}
{
}

template <>
Constraint* ScenarioComponentBase::make<Constraint, Scenario::ConstraintModel>(
    const Id<iscore::Component>& id, Scenario::ConstraintModel& elt)
{
  return new Constraint{m_constraintsNode, id, elt, system(), this};
}

template <>
Event* ScenarioComponentBase::make<Event, Scenario::EventModel>(
    const Id<iscore::Component>& id, Scenario::EventModel& elt)
{
  return new Event{m_eventsNode, id, elt, system(), this};
}

template <>
TimeNode* ScenarioComponentBase::make<TimeNode, Scenario::TimeNodeModel>(
    const Id<iscore::Component>& id, Scenario::TimeNodeModel& elt)
{
  return new TimeNode{m_timeNodesNode, id, elt, system(), this};
}

template <>
State* ScenarioComponentBase::make<State, Scenario::StateModel>(
    const Id<iscore::Component>& id, Scenario::StateModel& elt)
{
  return new State{m_statesNode, id, elt, system(), this};
}
}
}
