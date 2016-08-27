#include "LoopComponent.hpp"
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
        QObject* parent_obj):
    ProcessComponent_T<Loop::ProcessModel>{parent, loop, sys, id, "LoopComponent", parent_obj},
    m_constraintsNode{*node().createChild("constraints")},
    m_eventsNode{*node().createChild("events")},
    m_timeNodesNode{*node().createChild("timenodes")},
    m_statesNode{*node().createChild("states")}
{
}

template<>
Constraint* LoopComponentBase::make<Constraint, Scenario::ConstraintModel>(
        const Id<iscore::Component>& id,
        Scenario::ConstraintModel& elt)
{
    return new Constraint{m_constraintsNode, id, elt, system(), this};
}

template<>
Event* LoopComponentBase::make<Event, Scenario::EventModel>(
        const Id<iscore::Component>& id,
        Scenario::EventModel& elt)
{
    return new Event{m_eventsNode, id, elt, system(), this};
}

template<>
TimeNode* LoopComponentBase::make<TimeNode, Scenario::TimeNodeModel>(
        const Id<iscore::Component>& id,
        Scenario::TimeNodeModel& elt)
{
    return new TimeNode{m_timeNodesNode, id, elt, system(), this};
}

template<>
State* LoopComponentBase::make<State, Scenario::StateModel>(
        const Id<iscore::Component>& id,
        Scenario::StateModel& elt)
{
    return new State{m_statesNode, id, elt, system(), this};
}

}
}
