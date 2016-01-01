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
        const ScenarioComponent::system_t& doc,
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
ConstraintComponent* ScenarioComponent::make<ConstraintComponent, ConstraintModel>(
        const Id<iscore::Component>& id,
        ConstraintModel& elt,
        const ScenarioComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent)
{
    return new ConstraintComponent{*m_constraintsNode, id, elt, doc, ctx, parent};
}

template<>
EventComponent* ScenarioComponent::make<EventComponent, EventModel>(
        const Id<iscore::Component>& id,
        EventModel& elt,
        const ScenarioComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent)
{
    return new EventComponent{*m_eventsNode, id, elt, doc, ctx, parent};
}

template<>
TimeNodeComponent* ScenarioComponent::make<TimeNodeComponent, TimeNodeModel>(
        const Id<iscore::Component>& id,
        TimeNodeModel& elt,
        const ScenarioComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent)
{
    return new TimeNodeComponent{*m_timeNodesNode, id, elt, doc, ctx, parent};
}

template<>
StateComponent* ScenarioComponent::make<StateComponent, StateModel>(
        const Id<iscore::Component>& id,
        StateModel& elt,
        const ScenarioComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent)
{
    return new StateComponent{*m_statesNode, id, elt, doc, ctx, parent};
}

void ScenarioComponent::removing(const ConstraintModel& elt, const ConstraintComponent& comp)
{
    auto it = find_if(m_constraintsNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_constraintsNode->children().end());

    m_constraintsNode->eraseAndNotify(it);
}

void ScenarioComponent::removing(const EventModel& elt, const EventComponent& comp)
{
    auto it = find_if(m_eventsNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_eventsNode->children().end());

    m_eventsNode->eraseAndNotify(it);
}

void ScenarioComponent::removing(const TimeNodeModel& elt, const TimeNodeComponent& comp)
{
    auto it = find_if(m_timeNodesNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_timeNodesNode->children().end());

    m_timeNodesNode->eraseAndNotify(it);
}

void ScenarioComponent::removing(const StateModel& elt, const StateComponent& comp)
{
    auto it = find_if(m_statesNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_statesNode->children().end());

    m_statesNode->eraseAndNotify(it);
}

}
}
