#include <API/Headers/Editor/TimeConstraint.h>
#include <API/Headers/Editor/TimeEvent.h>
#include <API/Headers/Editor/TimeNode.h>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <algorithm>
#include <memory>
#include <vector>

#include "BaseScenarioElement.hpp"
#include "Editor/State.h"
#include "Editor/StateElement.h"
#include "Editor/TimeValue.h"
#include <OSSIA/Executor/ConstraintElement.hpp>
#include <OSSIA/Executor/EventElement.hpp>
#include <OSSIA/Executor/StateElement.hpp>
#include <OSSIA/Executor/TimeNodeElement.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>


namespace RecreateOnPlay
{

static void statusCallback(
        OSSIA::TimeEvent::Status newStatus)
{

}

static void baseScenarioConstraintCallback(const OSSIA::TimeValue&,
                               const OSSIA::TimeValue&,
                               std::shared_ptr<OSSIA::StateElement> element)
{
    element->launch();
}

BaseScenarioElement::BaseScenarioElement(
        const BaseScenario& element,
        QObject *parent):
    QObject{parent},
    m_deviceList{iscore::IDocument::documentContext(element).plugin<DeviceDocumentPlugin>().list()}
{
    auto main_start_node = OSSIA::TimeNode::create();
    auto main_end_node = OSSIA::TimeNode::create();
    auto main_start_event_it = main_start_node->emplace(main_start_node->timeEvents().begin(), statusCallback);
    auto main_end_event_it = main_end_node->emplace(main_end_node->timeEvents().begin(), statusCallback);
    auto main_start_state = OSSIA::State::create();
    auto main_end_state = OSSIA::State::create();

    (*main_start_event_it)->addState(main_start_state);
    (*main_end_event_it)->addState(main_end_state);

    OSSIA::TimeValue main_duration(iscore::convert::time(element.constraint().duration.defaultDuration()));

    // TODO PlayDuration of base constraint.
    // TODO PlayDuration of FullView
    auto main_constraint = OSSIA::TimeConstraint::create(
                               &baseScenarioConstraintCallback,
                               *main_start_event_it,
                               *main_end_event_it,
                               main_duration,
                               main_duration,
                               main_duration);

    // TODO put graphical settings somewhere.
    main_constraint->setSpeed(1.);
    main_constraint->setGranularity(50.);
    m_ossia_startTimeNode = new TimeNodeElement{main_start_node, element.startTimeNode(),  m_deviceList, this};
    m_ossia_endTimeNode = new TimeNodeElement{main_end_node, element.endTimeNode(), m_deviceList, this};

    m_ossia_startEvent = new EventElement{*main_start_event_it, element.startEvent(), m_deviceList, this};
    m_ossia_endEvent = new EventElement{*main_end_event_it, element.endEvent(), m_deviceList, this};

    m_ossia_startState = new StateElement{element.startState(), main_start_state, m_deviceList, this};
    m_ossia_endState = new StateElement{element.endState(), main_end_state, m_deviceList, this};

    m_ossia_constraint = new ConstraintElement{main_constraint, element.constraint(), this};
}

ConstraintElement *BaseScenarioElement::baseConstraint() const
{
    return m_ossia_constraint;
}

TimeNodeElement *BaseScenarioElement::startTimeNode() const
{
    return m_ossia_startTimeNode;
}

TimeNodeElement *BaseScenarioElement::endTimeNode() const
{
    return m_ossia_endTimeNode;
}

EventElement *BaseScenarioElement::startEvent() const
{
    return m_ossia_startEvent;
}

EventElement *BaseScenarioElement::endEvent() const
{
    return m_ossia_endEvent;
}

StateElement *BaseScenarioElement::startState() const
{
    return m_ossia_startState;
}

StateElement *BaseScenarioElement::endState() const
{
    return m_ossia_endState;
}
}
