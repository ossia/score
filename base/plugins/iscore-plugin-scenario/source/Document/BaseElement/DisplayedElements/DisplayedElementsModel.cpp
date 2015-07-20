#include "DisplayedElementsModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"

#include "Document/BaseElement/BaseScenario/BaseScenario.hpp"
#include "Process/ScenarioModel.hpp"
void DisplayedElementsModel::setSelection(const Selection & s)
{
    m_startEvent->selection.set(s.find(m_startEvent) != s.end());
    m_endEvent->selection.set(s.find(m_endEvent) != s.end());

    m_startState->selection.set(s.find(m_startState) != s.end());
    m_endState->selection.set(s.find(m_endState) != s.end());

    m_startNode->selection.set(s.find(m_startNode) != s.end());
    m_endNode->selection.set(s.find(m_endNode) != s.end());

    m_constraint->selection.set(s.find(m_constraint) != s.end());
 }

void DisplayedElementsModel::setDisplayedConstraint(const ConstraintModel *constraint)
{
    m_constraint = constraint;
    if(auto parent = dynamic_cast<BaseScenario*>(m_constraint->parent()))
    {
        m_startNode = &parent->startTimeNode();
        m_endNode = &parent->endTimeNode();

        m_startEvent = &parent->startEvent();
        m_endEvent = &parent->endEvent();

        m_startState = &parent->startState();
        m_endState = &parent->endState();
    }
    else if(auto parent = dynamic_cast<ScenarioModel*>(m_constraint->parent()))
    {
        m_startState = &parent->state(m_constraint->startState());
        m_endState = &parent->state(m_constraint->endState());

        m_startEvent = &parent->event(m_startState->eventId());
        m_endEvent = &parent->event(m_endState->eventId());

        m_startNode = &parent->timeNode(m_startEvent->timeNode());
        m_endNode = &parent->timeNode(m_endEvent->timeNode());
    }
}

const ConstraintModel &DisplayedElementsModel::displayedConstraint() const
{
    return *m_constraint;
}

const TimeNodeModel &DisplayedElementsModel::startNode() const
{
    return *m_startNode;
}

const TimeNodeModel &DisplayedElementsModel::endNode() const
{
    return *m_endNode;
}

const EventModel &DisplayedElementsModel::startEvent() const
{
    return *m_startEvent;
}

const EventModel &DisplayedElementsModel::endEvent() const
{
    return *m_endEvent;
}

const StateModel &DisplayedElementsModel::startState() const
{
    return *m_startState;
}

const StateModel &DisplayedElementsModel::endState() const
{
    return *m_endState;
}
