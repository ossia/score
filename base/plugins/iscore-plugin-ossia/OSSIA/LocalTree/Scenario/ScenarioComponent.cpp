#include "ScenarioComponent.hpp"

#include "MetadataParameters.hpp"
namespace Ossia
{
namespace LocalTree
{

ScenarioComponentBase::ScenarioComponentBase(
        const Id<Component>& id,
        OSSIA::Node& parent,
        Scenario::ProcessModel& scenario,
        system_t& sys,
        QObject* parent_obj):
    ProcessComponent_T{parent, scenario, id, "ScenarioComponent", parent_obj},
    m_sys{sys},
    m_constraintsNode{add_node(*node(), "constraints")},
    m_eventsNode{add_node(*node(), "events")},
    m_timeNodesNode{add_node(*node(), "timenodes")},
    m_statesNode{add_node(*node(), "states")}
{
    make_metadata_node(scenario.metadata, *node(), m_properties, this);
}

ScenarioComponentBase::~ScenarioComponentBase()
{
    m_properties.clear();

    m_constraintsNode.reset();
    m_eventsNode.reset();
    m_timeNodesNode.reset();
    m_statesNode.reset();

    m_thisNode.clear();
}

template<>
ConstraintComponent* ScenarioComponentBase::make<ConstraintComponent, Scenario::ConstraintModel>(
        const Id<iscore::Component>& id,
        Scenario::ConstraintModel& elt,
        QObject* parent)
{
    return new ConstraintComponent{*m_constraintsNode, id, elt, m_sys, parent};
}

template<>
EventComponent* ScenarioComponentBase::make<EventComponent, Scenario::EventModel>(
        const Id<iscore::Component>& id,
        Scenario::EventModel& elt,
        QObject* parent)
{
    return new EventComponent{*m_eventsNode, id, elt, m_sys, parent};
}

template<>
TimeNodeComponent* ScenarioComponentBase::make<TimeNodeComponent, Scenario::TimeNodeModel>(
        const Id<iscore::Component>& id,
        Scenario::TimeNodeModel& elt,
        QObject* parent)
{
    return new TimeNodeComponent{*m_timeNodesNode, id, elt, m_sys, parent};
}

template<>
StateComponent* ScenarioComponentBase::make<StateComponent, Scenario::StateModel>(
        const Id<iscore::Component>& id,
        Scenario::StateModel& elt,
        QObject* parent)
{
    return new StateComponent{*m_statesNode, id, elt, m_sys, parent};
}

void ScenarioComponentBase::removing(const Scenario::ConstraintModel& elt, const ConstraintComponent& comp)
{
    auto it = find_if(m_constraintsNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_constraintsNode->children().end());

    m_constraintsNode->erase(it);
}

void ScenarioComponentBase::removing(const Scenario::EventModel& elt, const EventComponent& comp)
{
    auto it = find_if(m_eventsNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_eventsNode->children().end());

    m_eventsNode->erase(it);
}

void ScenarioComponentBase::removing(const Scenario::TimeNodeModel& elt, const TimeNodeComponent& comp)
{
    auto it = find_if(m_timeNodesNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_timeNodesNode->children().end());

    m_timeNodesNode->erase(it);
}

void ScenarioComponentBase::removing(const Scenario::StateModel& elt, const StateComponent& comp)
{
    auto it = find_if(m_statesNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_statesNode->children().end());

    m_statesNode->erase(it);
}

}
}
