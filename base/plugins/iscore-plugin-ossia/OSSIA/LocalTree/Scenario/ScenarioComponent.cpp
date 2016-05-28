#include "ScenarioComponent.hpp"

#include "MetadataParameters.hpp"
namespace Ossia
{
namespace LocalTree
{

ScenarioComponent::ScenarioComponent(
        const Id<iscore::Component>& id,
        OSSIA::Node& parent,
        Scenario::ScenarioModel& scenario,
        system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent_obj):
    ProcessComponent{parent, scenario, id, "ScenarioComponent", parent_obj},
    m_constraintsNode{add_node(*node(), "constraints")},
    m_eventsNode{add_node(*node(), "events")},
    m_timeNodesNode{add_node(*node(), "timenodes")},
    m_statesNode{add_node(*node(), "states")},
    m_hm{*this, scenario, doc, ctx, this}
{
    make_metadata_node(scenario.metadata, *node(), m_properties, this);
}

template<>
ConstraintComponent* ScenarioComponent::make<ConstraintComponent, Scenario::ConstraintModel>(
        const Id<iscore::Component>& id,
        Scenario::ConstraintModel& elt,
        ScenarioComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent)
{
    return new ConstraintComponent{*m_constraintsNode, id, elt, doc, ctx, parent};
}

template<>
EventComponent* ScenarioComponent::make<EventComponent, Scenario::EventModel>(
        const Id<iscore::Component>& id,
        Scenario::EventModel& elt,
        ScenarioComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent)
{
    return new EventComponent{*m_eventsNode, id, elt, doc, ctx, parent};
}

template<>
TimeNodeComponent* ScenarioComponent::make<TimeNodeComponent, Scenario::TimeNodeModel>(
        const Id<iscore::Component>& id,
        Scenario::TimeNodeModel& elt,
        ScenarioComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent)
{
    return new TimeNodeComponent{*m_timeNodesNode, id, elt, doc, ctx, parent};
}

template<>
StateComponent* ScenarioComponent::make<StateComponent, Scenario::StateModel>(
        const Id<iscore::Component>& id,
        Scenario::StateModel& elt,
        ScenarioComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent)
{
    return new StateComponent{*m_statesNode, id, elt, doc, ctx, parent};
}

void ScenarioComponent::removing(const Scenario::ConstraintModel& elt, const ConstraintComponent& comp)
{
    auto it = find_if(m_constraintsNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_constraintsNode->children().end());

    m_constraintsNode->erase(it);
}

void ScenarioComponent::removing(const Scenario::EventModel& elt, const EventComponent& comp)
{
    auto it = find_if(m_eventsNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_eventsNode->children().end());

    m_eventsNode->erase(it);
}

void ScenarioComponent::removing(const Scenario::TimeNodeModel& elt, const TimeNodeComponent& comp)
{
    auto it = find_if(m_timeNodesNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_timeNodesNode->children().end());

    m_timeNodesNode->erase(it);
}

void ScenarioComponent::removing(const Scenario::StateModel& elt, const StateComponent& comp)
{
    auto it = find_if(m_statesNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_statesNode->children().end());

    m_statesNode->erase(it);
}

}
}
