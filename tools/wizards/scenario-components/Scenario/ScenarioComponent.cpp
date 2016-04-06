#include "ScenarioComponent.hpp"

namespace Audio
{
namespace AudioStreamEngine
{

ScenarioComponent::ScenarioComponent(
        const Id<iscore::Component>& id,
        Scenario::ScenarioModel& scenario,
        const ScenarioComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent_obj):
    ProcessComponent{scenario, id, "ScenarioComponent", parent_obj},
    m_hm{*this, scenario, doc, ctx, this}
{
}

template<>
ConstraintComponent* ScenarioComponent::make<ConstraintComponent, Scenario::ConstraintModel>(
        const Id<iscore::Component>& id,
        Scenario::ConstraintModel& elt,
        const ScenarioComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent)
{
    return new ConstraintComponent{id, elt, doc, ctx, parent};
}

template<>
EventComponent* ScenarioComponent::make<EventComponent, Scenario::EventModel>(
        const Id<iscore::Component>& id,
        Scenario::EventModel& elt,
        const ScenarioComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent)
{
    return new EventComponent{id, elt, doc, ctx, parent};
}

template<>
TimeNodeComponent* ScenarioComponent::make<TimeNodeComponent, Scenario::TimeNodeModel>(
        const Id<iscore::Component>& id,
        Scenario::TimeNodeModel& elt,
        const ScenarioComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent)
{
    return new TimeNodeComponent{id, elt, doc, ctx, parent};
}

template<>
StateComponent* ScenarioComponent::make<StateComponent, Scenario::StateModel>(
        const Id<iscore::Component>& id,
        Scenario::StateModel& elt,
        const ScenarioComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent)
{
    return new StateComponent{id, elt, doc, ctx, parent};
}

void ScenarioComponent::removing(const Scenario::ConstraintModel& elt, const ConstraintComponent& comp)
{
}

void ScenarioComponent::removing(const Scenario::EventModel& elt, const EventComponent& comp)
{
}

void ScenarioComponent::removing(const Scenario::TimeNodeModel& elt, const TimeNodeComponent& comp)
{
}

void ScenarioComponent::removing(const Scenario::StateModel& elt, const StateComponent& comp)
{
}

}
}
