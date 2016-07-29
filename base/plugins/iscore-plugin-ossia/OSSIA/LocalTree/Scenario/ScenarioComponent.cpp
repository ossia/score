#include "ScenarioComponent.hpp"

#include "MetadataParameters.hpp"
namespace Ossia
{
namespace LocalTree
{

ScenarioComponentBase::ScenarioComponentBase(
        const Id<iscore::Component>& id,
        OSSIA::net::Node& parent,
        Scenario::ProcessModel& scenario,
        DocumentPlugin& sys,
        QObject* parent_obj):
    ProcessComponent_T<Scenario::ProcessModel>{parent, scenario, sys, id, "ScenarioComponent", parent_obj},
    m_constraintsNode{*node().createChild("constraints")},
    m_eventsNode{*node().createChild("events")},
    m_timeNodesNode{*node().createChild("timenodes")},
    m_statesNode{*node().createChild("states")}
{
}

template<>
Constraint* ScenarioComponentBase::make<Constraint, Scenario::ConstraintModel>(
        const Id<iscore::Component>& id,
        Scenario::ConstraintModel& elt)
{
    return new Constraint{m_constraintsNode, id, elt, system(), this};
}

template<>
Event* ScenarioComponentBase::make<Event, Scenario::EventModel>(
        const Id<iscore::Component>& id,
        Scenario::EventModel& elt)
{
    return new Event{m_eventsNode, id, elt, system(), this};
}

template<>
TimeNode* ScenarioComponentBase::make<TimeNode, Scenario::TimeNodeModel>(
        const Id<iscore::Component>& id,
        Scenario::TimeNodeModel& elt)
{
    return new TimeNode{m_timeNodesNode, id, elt, system(), this};
}

template<>
State* ScenarioComponentBase::make<State, Scenario::StateModel>(
        const Id<iscore::Component>& id,
        Scenario::StateModel& elt)
{
    return new State{m_statesNode, id, elt, system(), this};
}

void ScenarioComponentBase::removing(const Scenario::ConstraintModel& elt, const Constraint& comp)
{
    m_constraintsNode.removeChild(comp.node());
}

void ScenarioComponentBase::removing(const Scenario::EventModel& elt, const Event& comp)
{
    m_eventsNode.removeChild(comp.node());
}

void ScenarioComponentBase::removing(const Scenario::TimeNodeModel& elt, const TimeNode& comp)
{
    m_timeNodesNode.removeChild(comp.node());
}

void ScenarioComponentBase::removing(const Scenario::StateModel& elt, const State& comp)
{
    m_statesNode.removeChild(comp.node());
}

}
}
