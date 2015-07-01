#include "OSSIABaseScenarioElement.hpp"

#include <Process/ScenarioModel.hpp>

#include <API/Headers/Editor/TimeConstraint.h>
#include <API/Headers/Editor/TimeEvent.h>
#include <API/Headers/Editor/TimeNode.h>

#include "Document/BaseElement/BaseScenario.hpp"
#include "iscore2OSSIA.hpp"
#include <QTimer>
OSSIABaseScenarioElement::OSSIABaseScenarioElement(const BaseScenario *element, QObject *parent)
{
    auto main_start_node = OSSIA::TimeNode::create();
    auto main_end_node = OSSIA::TimeNode::create();
    auto main_start_event_it = main_start_node->emplace(main_start_node->timeEvents().begin());
    auto main_end_event_it = main_end_node->emplace(main_end_node->timeEvents().begin());

    OSSIA::TimeValue main_duration(iscore::convert::time(element->baseConstraint()->defaultDuration()));
    auto main_constraint = OSSIA::TimeConstraint::create(*main_start_event_it, *main_end_event_it, main_duration);

    m_ossia_startTimeNode = new OSSIATimeNodeElement{main_start_node, *element->startTimeNode(), element->startTimeNode()};
    m_ossia_endTimeNode = new OSSIATimeNodeElement{main_end_node, *element->endTimeNode(), element->endTimeNode()};

    m_ossia_startEvent = new OSSIAEventElement{*main_start_event_it, element->startEvent(), element->startEvent()};
    m_ossia_endEvent = new OSSIAEventElement{*main_end_event_it, element->endEvent(), element->endEvent()};

    m_ossia_constraint = new OSSIAConstraintElement{main_constraint, *element->baseConstraint(), element->baseConstraint()};

    element->startTimeNode()->pluginModelList.add(m_ossia_startTimeNode);
    element->endTimeNode()->pluginModelList.add(m_ossia_endTimeNode);

    element->startEvent()->pluginModelList.add(m_ossia_startEvent);
    element->endEvent()->pluginModelList.add(m_ossia_endEvent);

    element->baseConstraint()->pluginModelList.add(m_ossia_constraint);
}

iscore::ElementPluginModelType OSSIABaseScenarioElement::elementPluginId() const
{
    return staticPluginId();
}

iscore::ElementPluginModel *OSSIABaseScenarioElement::clone(const QObject *element, QObject *parent) const
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
    return nullptr;
}

void OSSIABaseScenarioElement::serialize(const VisitorVariant &) const
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
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
