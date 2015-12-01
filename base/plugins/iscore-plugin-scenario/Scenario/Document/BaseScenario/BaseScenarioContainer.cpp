#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <boost/optional/optional.hpp>

#include "BaseScenarioContainer.hpp"
#include <Process/TimeValue.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <core/document/DocumentContext.hpp>
class ConstraintViewModel;

BaseScenarioContainer::~BaseScenarioContainer()
{

}

void BaseScenarioContainer::init()
{
    auto& stack = iscore::IDocument::documentContext(*m_parent).commandStack;
    m_startNode = new TimeNodeModel{Id<TimeNodeModel>{0}, {0.2, 0.8}, TimeValue::zero(),  m_parent};
    m_endNode = new TimeNodeModel{Id<TimeNodeModel>{1}, {0.2, 0.8}, TimeValue::zero(), m_parent};

    m_startEvent = new EventModel{Id<EventModel>{0}, m_startNode->id(), {0.4, 0.6}, TimeValue::zero(), m_parent};
    m_endEvent = new EventModel{Id<EventModel>{1}, m_endNode->id(),   {0.4, 0.6}, TimeValue::zero(), m_parent};

    m_startState = new StateModel{Id<StateModel>{0}, m_startEvent->id(), 0, stack, m_parent};
    m_endState = new StateModel{Id<StateModel>{1}, m_endEvent->id(),   0, stack, m_parent};

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

    m_startState->setNextConstraint(m_constraint->id());
    m_endState->setPreviousConstraint(m_constraint->id());

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
