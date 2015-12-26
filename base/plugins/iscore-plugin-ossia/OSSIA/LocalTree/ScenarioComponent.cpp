#include "ScenarioComponent.hpp"

namespace OSSIA
{
namespace LocalTree
{

ScenarioComponent::ScenarioComponent(
        const Id<iscore::Component>& id,
        Node& parent,
        Scenario::ScenarioModel& scenario,
        const ScenarioComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent_obj):
    ProcessComponent{
        add_node(parent, scenario.metadata.name().toStdString()),
        id, "ScenarioComponent", parent_obj},
    m_constraintsNode{add_node(*node(), "constraints")},
    m_eventsNode{add_node(*node(), "events")},
    m_timeNodesNode{add_node(*node(), "timenodes")},
    m_statesNode{add_node(*node(), "states")},
    m_hm{*this, scenario, doc, ctx, this}
{
    make_metadata_node(scenario.metadata, *node(), m_properties, this);
}

ConstraintComponent*ScenarioComponent::makeConstraint(const Id<iscore::Component>& id, ConstraintModel& elt, const ScenarioComponent::system_t& doc, const iscore::DocumentContext& ctx, QObject* parent)
{
    return new ConstraintComponent{*m_constraintsNode, id, elt, doc, ctx, parent};
}

EventComponent*ScenarioComponent::makeEvent(const Id<iscore::Component>& id, EventModel& elt, const ScenarioComponent::system_t& doc, const iscore::DocumentContext& ctx, QObject* parent)
{
    return new EventComponent{*m_eventsNode, id, elt, doc, ctx, parent};
}

TimeNodeComponent*ScenarioComponent::makeTimeNode(const Id<iscore::Component>& id, TimeNodeModel& elt, const ScenarioComponent::system_t& doc, const iscore::DocumentContext& ctx, QObject* parent)
{
    return new TimeNodeComponent{*m_timeNodesNode, id, elt, doc, ctx, parent};
}

StateComponent*ScenarioComponent::makeState(const Id<iscore::Component>& id, StateModel& elt, const ScenarioComponent::system_t& doc, const iscore::DocumentContext& ctx, QObject* parent)
{
    return new StateComponent{*m_statesNode, id, elt, doc, ctx, parent};
}

void ScenarioComponent::removing(const ConstraintModel& elt, const ConstraintComponent& comp)
{
    auto it = find_if(m_constraintsNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_constraintsNode->children().end());

    m_constraintsNode->children().erase(it);
}

void ScenarioComponent::removing(const EventModel& elt, const EventComponent& comp)
{
    auto it = find_if(m_eventsNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_eventsNode->children().end());

    m_eventsNode->children().erase(it);
}

void ScenarioComponent::removing(const TimeNodeModel& elt, const TimeNodeComponent& comp)
{
    auto it = find_if(m_timeNodesNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_timeNodesNode->children().end());

    m_timeNodesNode->children().erase(it);
}

void ScenarioComponent::removing(const StateModel& elt, const StateComponent& comp)
{
    auto it = find_if(m_statesNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_statesNode->children().end());

    m_statesNode->children().erase(it);
}

}
}
