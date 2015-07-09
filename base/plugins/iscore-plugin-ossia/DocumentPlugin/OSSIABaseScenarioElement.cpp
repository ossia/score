#include "OSSIABaseScenarioElement.hpp"

#include <Process/ScenarioModel.hpp>

#include <API/Headers/Editor/TimeConstraint.h>
#include <API/Headers/Editor/TimeEvent.h>
#include <API/Headers/Editor/TimeNode.h>

#include "Document/BaseElement/BaseScenario/BaseScenarioModel.hpp"
#include "iscore2OSSIA.hpp"
#include <QTimer>

static void statusCallback(OSSIA::TimeEvent::Status newStatus, OSSIA::TimeEvent::Status oldStatus)
{

}

OSSIABaseScenarioElement::OSSIABaseScenarioElement(const BaseScenario *element, QObject *parent)
{
    auto main_start_node = OSSIA::TimeNode::create();
    auto main_end_node = OSSIA::TimeNode::create();
    auto main_start_event_it = main_start_node->emplace(main_start_node->timeEvents().begin(), statusCallback);
    auto main_end_event_it = main_end_node->emplace(main_end_node->timeEvents().begin(), statusCallback);

    OSSIA::TimeValue main_duration(iscore::convert::time(element->baseConstraint()->defaultDuration()));
    auto main_constraint = OSSIA::TimeConstraint::create(*main_start_event_it, *main_end_event_it, main_duration);

    m_ossia_startTimeNode = new OSSIATimeNodeElement{main_start_node, *element->startTimeNode(), this};
    m_ossia_endTimeNode = new OSSIATimeNodeElement{main_end_node, *element->endTimeNode(), this};

    m_ossia_startEvent = new OSSIAEventElement{*main_start_event_it, element->startEvent(), this};
    m_ossia_endEvent = new OSSIAEventElement{*main_end_event_it, element->endEvent(), this};

    m_ossia_startState = new OSSIAStateElement{element->startState(), this};
    m_ossia_endState = new OSSIAStateElement{element->endState(), this};

    m_ossia_constraint = new OSSIAConstraintElement{main_constraint, *element->baseConstraint(), this};
}

OSSIAConstraintElement *OSSIABaseScenarioElement::baseConstraint() const
{
    return m_ossia_constraint;
}

OSSIATimeNodeElement *OSSIABaseScenarioElement::startTimeNode() const
{
    return m_ossia_startTimeNode;
}

OSSIATimeNodeElement *OSSIABaseScenarioElement::endTimeNode() const
{
    return m_ossia_endTimeNode;
}

OSSIAEventElement *OSSIABaseScenarioElement::startEvent() const
{
    return m_ossia_startEvent;
}

OSSIAEventElement *OSSIABaseScenarioElement::endEvent() const
{
    return m_ossia_endEvent;
}
