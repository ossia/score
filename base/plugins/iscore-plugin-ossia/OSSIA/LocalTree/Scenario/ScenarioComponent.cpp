#include "ScenarioComponent.hpp"

#include "MetadataParameters.hpp"
namespace Ossia
{
namespace LocalTree
{

ScenarioComponentBase::ScenarioComponentBase(
        const Id<iscore::Component>& id,
        OSSIA::Node& parent,
        Scenario::ProcessModel& scenario,
        DocumentPlugin& sys,
        QObject* parent_obj):
    ProcessComponent_T<Scenario::ProcessModel>{parent, scenario, sys, id, "ScenarioComponent", parent_obj},
    m_constraintsNode{add_node(*node(), "constraints")},
    m_eventsNode{add_node(*node(), "events")},
    m_timeNodesNode{add_node(*node(), "timenodes")},
    m_statesNode{add_node(*node(), "states")}
{
}

template<>
Constraint* ScenarioComponentBase::make<Constraint, Scenario::ConstraintModel>(
        const Id<iscore::Component>& id,
        Scenario::ConstraintModel& elt)
{
    return new Constraint{*m_constraintsNode, id, elt, system(), this};
}

template<>
Event* ScenarioComponentBase::make<Event, Scenario::EventModel>(
        const Id<iscore::Component>& id,
        Scenario::EventModel& elt)
{
    return new Event{*m_eventsNode, id, elt, system(), this};
}

template<>
TimeNode* ScenarioComponentBase::make<TimeNode, Scenario::TimeNodeModel>(
        const Id<iscore::Component>& id,
        Scenario::TimeNodeModel& elt)
{
    return new TimeNode{*m_timeNodesNode, id, elt, system(), this};
}

template<>
State* ScenarioComponentBase::make<State, Scenario::StateModel>(
        const Id<iscore::Component>& id,
        Scenario::StateModel& elt)
{
    return new State{*m_statesNode, id, elt, system(), this};
}

void ScenarioComponentBase::removing(const Scenario::ConstraintModel& elt, const Constraint& comp)
{
    auto it = find_if(m_constraintsNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_constraintsNode->children().end());

    m_constraintsNode->erase(it);
}

void ScenarioComponentBase::removing(const Scenario::EventModel& elt, const Event& comp)
{
    auto it = find_if(m_eventsNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_eventsNode->children().end());

    m_eventsNode->erase(it);
}

void ScenarioComponentBase::removing(const Scenario::TimeNodeModel& elt, const TimeNode& comp)
{
    auto it = find_if(m_timeNodesNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_timeNodesNode->children().end());

    m_timeNodesNode->erase(it);
}

void ScenarioComponentBase::removing(const Scenario::StateModel& elt, const State& comp)
{
    auto it = find_if(m_statesNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_statesNode->children().end());

    m_statesNode->erase(it);
}

}
}
