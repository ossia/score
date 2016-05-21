#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>

#include <iscore/tools/std/Optional.hpp>

#include "BaseScenarioContainer.hpp"
#include <Process/TimeValue.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/document/DocumentContext.hpp>
namespace Scenario
{
class ConstraintViewModel;

BaseScenarioContainer::~BaseScenarioContainer() = default;

void BaseScenarioContainer::init()
{
    auto& stack = iscore::IDocument::documentContext(*m_parent).commandStack;
    m_startNode = new TimeNodeModel{Scenario::startId<TimeNodeModel>(), {0.2, 0.8}, TimeValue::zero(),  m_parent};
    m_endNode = new TimeNodeModel{Scenario::endId<TimeNodeModel>(), {0.2, 0.8}, TimeValue::zero(), m_parent};

    m_startEvent = new EventModel{Scenario::startId<EventModel>(), m_startNode->id(), {0.1, 0.15}, TimeValue::zero(), m_parent};
    m_endEvent = new EventModel{Scenario::endId<EventModel>(), m_endNode->id(),   {0.4, 0.6}, TimeValue::zero(), m_parent};

    m_startState = new StateModel{Scenario::startId<StateModel>(), m_startEvent->id(), 0, stack, m_parent};
    m_endState = new StateModel{Scenario::endId<StateModel>(), m_endEvent->id(),   0, stack, m_parent};

    m_constraint = new ConstraintModel{
            Id<ConstraintModel>{0},
            Id<ConstraintViewModel>{0},
            0,
            m_parent};

    m_startNode->addEvent(m_startEvent->id());
    m_endNode->addEvent(m_endEvent->id());

    m_startEvent->addState(m_startState->id());
    m_endEvent->addState(m_endState->id());

    m_constraint->setStartState(m_startState->id());
    m_constraint->setEndState(m_endState->id());

    SetNextConstraint(*m_startState, *m_constraint);
    SetPreviousConstraint(*m_endState, *m_constraint);
}

void BaseScenarioContainer::init(const BaseScenarioContainer& source)
{
    auto& stack = iscore::IDocument::documentContext(*m_parent).commandStack;
    m_constraint = new ConstraintModel{*source.m_constraint, source.m_constraint->id(), m_parent};

    m_startNode = new TimeNodeModel{*source.m_startNode, source.m_startNode->id(),  m_parent};
    m_endNode = new TimeNodeModel{*source.m_endNode, source.m_endNode->id(), m_parent};

    m_startEvent = new EventModel{*source.m_startEvent, source.m_startEvent->id(), m_parent};
    m_endEvent = new EventModel{*source.m_endEvent, source.m_endEvent->id(), m_parent};

    m_startState = new StateModel{*source.m_startState, source.m_startState->id(), stack, m_parent};
    m_endState = new StateModel{*source.m_endState, source.m_endState->id(),  stack, m_parent};

    SetPreviousConstraint(*m_endState, *m_constraint);
    SetNextConstraint(*m_startState, *m_constraint);
}

ConstraintModel* BaseScenarioContainer::findConstraint(
        const Id<ConstraintModel>& id) const
{
    if(id == m_constraint->id())
        return m_constraint;
    return nullptr;
}

EventModel* BaseScenarioContainer::findEvent(
        const Id<EventModel>& id) const
{
    if(id == m_startEvent->id())
    {
        return m_startEvent;
    }
    else if(id == m_endEvent->id())
    {
        return m_endEvent;
    }
    else
    {
        return nullptr;
    }
}

TimeNodeModel*BaseScenarioContainer::findTimeNode(
        const Id<TimeNodeModel>& id) const
{
    if(id == m_startNode->id())
    {
        return m_startNode;
    }
    else if(id == m_endNode->id())
    {
        return m_endNode;
    }
    else
    {
        return nullptr;
    }
}

StateModel* BaseScenarioContainer::findState(
        const Id<StateModel>& id) const
{
    if(id == m_startState->id())
    {
        return m_startState;
    }
    else if(id == m_endState->id())
    {
        return m_endState;
    }
    else
    {
        return nullptr;
    }
}

ConstraintModel& BaseScenarioContainer::constraint(
        const Id<ConstraintModel>& id) const
{
    ISCORE_ASSERT(id == m_constraint->id());
    return *m_constraint;
}

EventModel& BaseScenarioContainer::event(
        const Id<EventModel>& id) const
{
    ISCORE_ASSERT(id == m_startEvent->id() || id == m_endEvent->id());
    return id == m_startEvent->id()
            ? *m_startEvent
            : *m_endEvent;
}

TimeNodeModel& BaseScenarioContainer::timeNode(
        const Id<TimeNodeModel>& id) const
{
    ISCORE_ASSERT(id == m_startNode->id() || id == m_endNode->id());
    return id == m_startNode->id()
            ? *m_startNode
            : *m_endNode;
}

StateModel& BaseScenarioContainer::state(
        const Id<StateModel>& id) const
{
    ISCORE_ASSERT(id == m_startState->id() || id == m_endState->id());
    return id == m_startState->id()
            ? *m_startState
            : *m_endState;
}

ConstraintModel& BaseScenarioContainer::constraint() const
{
    return *m_constraint;
}

TimeNodeModel& BaseScenarioContainer::startTimeNode() const
{
    return *m_startNode;
}

TimeNodeModel& BaseScenarioContainer::endTimeNode() const
{
    return *m_endNode;
}

EventModel& BaseScenarioContainer::startEvent() const
{
    return *m_startEvent;
}

EventModel& BaseScenarioContainer::endEvent() const
{
    return *m_endEvent;
}

StateModel& BaseScenarioContainer::startState() const
{
    return *m_startState;
}

StateModel& BaseScenarioContainer::endState() const
{
    return *m_endState;
}
}
