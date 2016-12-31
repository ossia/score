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

#include "BaseScenarioComponent.hpp"
#include <Engine/Executor/ConstraintComponent.hpp>
#include <Engine/Executor/EventComponent.hpp>
#include <Engine/Executor/StateComponent.hpp>
#include <Engine/Executor/TimeNodeComponent.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <Engine/Executor/ExecutorContext.hpp>

#include <Engine/OSSIA2iscore.hpp>

namespace Engine
{
namespace Execution
{
BaseScenarioElement::BaseScenarioElement(
    BaseScenarioRefContainer element, const Context& ctx, QObject* parent)
    : QObject{nullptr}, m_ctx{ctx}
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

  m_ossia_startTimeNode = std::make_shared<TimeNodeComponent>(
      element.startTimeNode(), m_ctx, newId(element.startTimeNode()), this);
  m_ossia_endTimeNode = std::make_shared<TimeNodeComponent>(
      element.endTimeNode(), m_ctx, newId(element.endTimeNode()), this);

  m_ossia_startEvent = std::make_shared<EventComponent>(element.startEvent(), m_ctx, newId(element.startEvent()), this);
  m_ossia_endEvent = std::make_shared<EventComponent>(element.endEvent(), m_ctx, newId(element.endEvent()), this);

  m_ossia_startState = std::make_shared<StateComponent>(element.startState(), m_ctx, newId(element.startState()), this);
  m_ossia_endState =  std::make_shared<StateComponent>(element.endState(), m_ctx, newId(element.endState()), this);

  m_ossia_constraint = std::make_shared<ConstraintComponent>(
      element.constraint(), m_ctx, newId(element.constraint()), this);

  m_ossia_startTimeNode->onSetup(main_start_node, m_ossia_startTimeNode->makeTrigger());
  m_ossia_endTimeNode->onSetup(main_end_node, m_ossia_endTimeNode->makeTrigger());
  m_ossia_startEvent->onSetup(main_start_event, m_ossia_startEvent->makeExpression(), (ossia::time_event::OffsetBehavior)element.startEvent().offsetBehavior());
  m_ossia_endEvent->onSetup(main_end_event, m_ossia_endEvent->makeExpression(), (ossia::time_event::OffsetBehavior)element.endEvent().offsetBehavior());
  m_ossia_startState->onSetup(main_start_event);
  m_ossia_endState->onSetup(main_end_event);
  m_ossia_constraint->onSetup(main_constraint, m_ossia_constraint->makeDurations(), true);

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
}

void BaseScenarioElement::cleanup()
{
  if(m_ossia_constraint)
    m_ossia_constraint->cleanup();
  if(m_ossia_startState)
    m_ossia_startState->cleanup();
  if(m_ossia_endState)
    m_ossia_endState->cleanup();
  if(m_ossia_startEvent)
    m_ossia_startEvent->cleanup();
  if(m_ossia_endEvent)
    m_ossia_endEvent->cleanup();
  if(m_ossia_startTimeNode)
  {
    m_ossia_startTimeNode->OSSIATimeNode()->cleanup();
    m_ossia_startTimeNode->cleanup();
  }
  if(m_ossia_endTimeNode)
  {
    m_ossia_endTimeNode->OSSIATimeNode()->cleanup();
    m_ossia_endTimeNode->cleanup();
  }

  m_ossia_constraint.reset();
  m_ossia_startState.reset();
  m_ossia_endState.reset();
  m_ossia_startEvent.reset();
  m_ossia_endEvent.reset();
  m_ossia_startTimeNode.reset();
  m_ossia_endTimeNode.reset();

  m_ctx.sys.runAllCommands();
}

ConstraintComponent& BaseScenarioElement::baseConstraint() const
{
  return *m_ossia_constraint;
}

TimeNodeComponent& BaseScenarioElement::startTimeNode() const
{
  return *m_ossia_startTimeNode;
}

TimeNodeComponent& BaseScenarioElement::endTimeNode() const
{
  return *m_ossia_endTimeNode;
}

EventComponent& BaseScenarioElement::startEvent() const
{
  return *m_ossia_startEvent;
}

EventComponent& BaseScenarioElement::endEvent() const
{
  return *m_ossia_endEvent;
}

StateComponent& BaseScenarioElement::startState() const
{
  return *m_ossia_startState;
}

StateComponent& BaseScenarioElement::endState() const
{
  return *m_ossia_endState;
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
