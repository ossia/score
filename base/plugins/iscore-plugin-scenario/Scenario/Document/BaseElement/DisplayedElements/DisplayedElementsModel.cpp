#include "DisplayedElementsModel.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>

#include <Scenario/Document/BaseElement/BaseScenario/BaseScenario.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
void DisplayedElementsModel::setSelection(const Selection & s)
{
    bool startev{}, endev{}, starttn{}, endtn{}, startst{}, endst{}, cst{};
    for(const auto& elt : s)
    {
        ISCORE_ASSERT(elt);

        if(elt == m_startEvent)
            startev = true;
        else if(elt == m_endEvent)
            endev = true;
        else if(elt == m_startNode)
            starttn = true;
        else if(elt == m_endNode)
            endtn = true;
        else if(elt == m_startState)
            startst = true;
        else if(elt == m_endState)
            endst = true;
        else if(elt == m_constraint)
            cst = true;
    }

    if(m_startEvent) m_startEvent->selection.set(startev);
    if(m_endEvent) m_endEvent->selection.set(endev);

    if(m_startState) m_startState->selection.set(starttn);
    if(m_endState) m_endState->selection.set(endtn);

    if(m_startNode) m_startState->selection.set(startst);
    if(m_endNode) m_endState->selection.set(endst);

    if(m_constraint) m_endState->selection.set(cst);
 }

void DisplayedElementsModel::setDisplayedConstraint(const ConstraintModel& constraint)
{
    m_constraint = &constraint;
    if(auto parent_base = dynamic_cast<BaseScenario*>(m_constraint->parent()))
    {
        m_startNode = &parent_base->startTimeNode();
        m_endNode = &parent_base->endTimeNode();

        m_startEvent = &parent_base->startEvent();
        m_endEvent = &parent_base->endEvent();

        m_startState = &parent_base->startState();
        m_endState = &parent_base->endState();
    }
    else if(auto parent_scenario = dynamic_cast<Scenario::ScenarioModel*>(m_constraint->parent()))
    {
        m_startState = &parent_scenario->states.at(m_constraint->startState());
        m_endState = &parent_scenario->states.at(m_constraint->endState());

        m_startEvent = &parent_scenario->events.at(m_startState->eventId());
        m_endEvent = &parent_scenario->events.at(m_endState->eventId());

        m_startNode = &parent_scenario->timeNodes.at(m_startEvent->timeNode());
        m_endNode = &parent_scenario->timeNodes.at(m_endEvent->timeNode());
    }
}

const ConstraintModel &DisplayedElementsModel::constraint() const
{
    return *m_constraint;
}

const TimeNodeModel &DisplayedElementsModel::startTimeNode() const
{
    return *m_startNode;
}

const TimeNodeModel &DisplayedElementsModel::endTimeNode() const
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
