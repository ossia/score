#include <ossia/editor/scenario/time_constraint.hpp>
#include <ossia/editor/scenario/time_event.hpp>
#include <ossia/editor/scenario/time_node.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/editor/state/state.hpp>
#include <ossia/editor/state/state_element.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <algorithm>
#include <iscore/document/DocumentInterface.hpp>
#include <vector>

#include "BaseScenarioElement.hpp"
#include <Engine/Executor/ConstraintElement.hpp>
#include <Engine/Executor/EventElement.hpp>
#include <Engine/Executor/StateElement.hpp>
#include <Engine/Executor/TimeNodeElement.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>

#include <Engine/Executor/ExecutorContext.hpp>

#include <Engine/OSSIA2iscore.hpp>

namespace Engine
{
namespace Execution
{
BaseScenarioElement::BaseScenarioElement(
    BaseScenarioRefContainer element, const Context& ctx, QObject* parent)
    : QObject{parent}, m_ctx{ctx}
{
  auto main_start_node = std::make_shared<ossia::time_node>();
  auto main_end_node = std::make_shared<ossia::time_node>();

  auto main_start_event = *main_start_node->emplace(
      main_start_node->timeEvents().begin(),
      [this](auto&&...) {},
      ossia::expressions::make_expression_true());
  auto main_end_event = *main_end_node->emplace(
      main_end_node->timeEvents().begin(),
      [this](auto&&...) {},
      ossia::expressions::make_expression_true());

  // TODO PlayDuration of base constraint.
  // TODO PlayDuration of FullView
  auto main_constraint = ossia::time_constraint::create(
      [](auto&&...) {},
      *main_start_event,
      *main_end_event,
      Engine::iscore_to_ossia::time(
          element.constraint().duration.defaultDuration()),
      Engine::iscore_to_ossia::time(
          element.constraint().duration.minDuration()),
      Engine::iscore_to_ossia::time(
          element.constraint().duration.maxDuration()));

  m_ossia_startTimeNode = new TimeNodeElement{
      main_start_node, element.startTimeNode(), m_ctx.devices.list(), this};
  m_ossia_endTimeNode = new TimeNodeElement{
      main_end_node, element.endTimeNode(), m_ctx.devices.list(), this};

  m_ossia_startEvent = new EventElement{main_start_event, element.startEvent(),
                                        m_ctx.devices.list(), this};
  m_ossia_endEvent = new EventElement{main_end_event, element.endEvent(),
                                      m_ctx.devices.list(), this};

  m_ossia_startState
      = new StateElement{element.startState(), *main_start_event, m_ctx, this};
  m_ossia_endState
      = new StateElement{element.endState(), *main_end_event, m_ctx, this};

  m_ossia_constraint = new ConstraintElement{
      main_constraint, element.constraint(), m_ctx, this};

  main_constraint->setExecutionStatusCallback(
      [=](ossia::clock::ClockExecutionStatus c) {
        if (c == ossia::clock::ClockExecutionStatus::STOPPED)
        {
          ossia::state accumulator;
          ossia::flatten_and_filter(accumulator, main_end_event->getState());
          accumulator.launch();

          emit finished();
        }
      });
}

BaseScenarioElement::~BaseScenarioElement()
{
  m_ossia_startTimeNode->OSSIATimeNode()->cleanup();
  m_ossia_endTimeNode->OSSIATimeNode()->cleanup();
}

ConstraintElement* BaseScenarioElement::baseConstraint() const
{
  return m_ossia_constraint;
}

TimeNodeElement* BaseScenarioElement::startTimeNode() const
{
  return m_ossia_startTimeNode;
}

TimeNodeElement* BaseScenarioElement::endTimeNode() const
{
  return m_ossia_endTimeNode;
}

EventElement* BaseScenarioElement::startEvent() const
{
  return m_ossia_startEvent;
}

EventElement* BaseScenarioElement::endEvent() const
{
  return m_ossia_endEvent;
}

StateElement* BaseScenarioElement::startState() const
{
  return m_ossia_startState;
}

StateElement* BaseScenarioElement::endState() const
{
  return m_ossia_endState;
}
}
}

BaseScenarioRefContainer::BaseScenarioRefContainer(
    Scenario::ConstraintModel& constraint, Scenario::ScenarioInterface& s)
    : m_constraint{constraint}
    , m_startState{s.state(constraint.startState())}
    , m_endState{s.state(constraint.endState())}
    , m_startEvent{s.event(m_startState.eventId())}
    , m_endEvent{s.event(m_endState.eventId())}
    , m_startNode{s.timeNode(m_startEvent.timeNode())}
    , m_endNode{s.timeNode(m_endEvent.timeNode())}
{
}
