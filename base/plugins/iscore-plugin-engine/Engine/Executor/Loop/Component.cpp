// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/editor/scenario/time_event.hpp>
#include <ossia/editor/scenario/time_node.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <algorithm>
#include <core/document/Document.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <vector>

#include "Component.hpp"
#include <ossia/editor/loop/loop.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/editor/state/state.hpp>
#include <Engine/Executor/ConstraintComponent.hpp>
#include <Engine/Executor/EventComponent.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <Engine/Executor/StateComponent.hpp>
#include <Engine/Executor/TimeNodeComponent.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>

namespace Process
{
class ProcessModel;
}
class QObject;
namespace ossia
{
class time_process;
} // namespace OSSIA
#include <iscore/model/Identifier.hpp>

namespace Loop
{
namespace RecreateOnPlay
{
Component::Component(
    ::Engine::Execution::ConstraintComponent& parentConstraint,
    ::Loop::ProcessModel& element,
    const ::Engine::Execution::Context& ctx,
    const Id<iscore::Component>& id,
    QObject* parent)
    : ::Engine::Execution::ProcessComponent_T<Loop::ProcessModel, ossia::loop>{
          parentConstraint, element, ctx, id, "LoopComponent", parent}
{
  ossia::time_value main_duration(ctx.time(
      element.constraint().duration.defaultDuration()));

  auto loop = std::make_shared<ossia::loop>(
      main_duration,
      [](double, ossia::time_value, const ossia::state_element&) {},
      [this, &element](ossia::time_event::status newStatus) {

        element.startEvent().setStatus(
            static_cast<Scenario::ExecutionStatus>(newStatus), process());
        switch (newStatus)
        {
          case ossia::time_event::status::NONE:
            break;
          case ossia::time_event::status::PENDING:
            break;
          case ossia::time_event::status::HAPPENED:
            startConstraintExecution(
                m_ossia_constraint->iscoreConstraint().id());
            break;
          case ossia::time_event::status::DISPOSED:
            break;
          default:
            ISCORE_TODO;
            break;
        }
      },
      [this, &element](ossia::time_event::status newStatus) {

        element.endEvent().setStatus(
            static_cast<Scenario::ExecutionStatus>(newStatus), process());
        switch (newStatus)
        {
          case ossia::time_event::status::NONE:
            break;
          case ossia::time_event::status::PENDING:
            break;
          case ossia::time_event::status::HAPPENED:
            stopConstraintExecution(
                m_ossia_constraint->iscoreConstraint().id());
            break;
          case ossia::time_event::status::DISPOSED:
            break;
          default:
            ISCORE_TODO;
            break;
        }

      });

  m_ossia_process = loop;

  // TODO also states in BasEelement
  // TODO put graphical settings somewhere.
  auto main_start_node = loop->get_start_timenode();
  auto main_end_node = loop->get_end_timenode();
  auto main_start_event = *main_start_node->get_time_events().begin();
  auto main_end_event = *main_end_node->get_time_events().begin();

  using namespace Engine::Execution;
  m_ossia_startTimeNode = new TimeNodeComponent(element.startTimeNode(),
                                              system(), iscore::newId(element.startTimeNode()), this);
  m_ossia_endTimeNode = new TimeNodeComponent(element.endTimeNode(),
                                            system(), iscore::newId(element.endTimeNode()), this);

  m_ossia_startEvent = new EventComponent(element.startEvent(),
                                        system(), iscore::newId(element.startEvent()), this);
  m_ossia_endEvent = new EventComponent(element.endEvent(),
                                      system(), iscore::newId(element.endEvent()), this);

  m_ossia_startState
      = new StateComponent(element.startState(), system(), iscore::newId(element.startState()), this);
  m_ossia_endState
      = new StateComponent(element.endState(), system(), iscore::newId(element.endState()), this);


  m_ossia_constraint = new ConstraintComponent(element.constraint(), system(), iscore::newId(element.constraint()), this);

  m_ossia_startTimeNode->onSetup(main_start_node, m_ossia_startTimeNode->makeTrigger());
  m_ossia_endTimeNode->onSetup(main_end_node, m_ossia_endTimeNode->makeTrigger());
  m_ossia_startEvent->onSetup(main_start_event, m_ossia_startEvent->makeExpression(), (ossia::time_event::offset_behavior)element.startEvent().offsetBehavior());
  m_ossia_endEvent->onSetup(main_end_event, m_ossia_endEvent->makeExpression(), (ossia::time_event::offset_behavior)element.endEvent().offsetBehavior());
  m_ossia_startState->onSetup(main_start_event);
  m_ossia_endState->onSetup(main_end_event);
  m_ossia_constraint->onSetup(loop->get_time_constraint(), m_ossia_constraint->makeDurations(), false);

  element.startState().components().add(m_ossia_startState);
  element.endState().components().add(m_ossia_endState);

  element.startEvent().components().add(m_ossia_startEvent);
  element.endEvent().components().add(m_ossia_endEvent);

  element.startTimeNode().components().add(m_ossia_startTimeNode);
  element.endTimeNode().components().add(m_ossia_endTimeNode);

  element.constraint().components().add(m_ossia_constraint);
}

Component::~Component()
{
}

void Component::cleanup()
{
  if(m_ossia_constraint)
  {
    m_ossia_constraint->cleanup();
    process().constraint().components().remove(m_ossia_constraint);
  }
  if(m_ossia_startState)
  {
    m_ossia_startState->cleanup();
    process().startState().components().remove(m_ossia_startState);
  }
  if(m_ossia_endState)
  {
    m_ossia_endState->cleanup();
    process().endState().components().remove(m_ossia_endState);
  }
  if(m_ossia_startEvent)
  {
    m_ossia_startEvent->cleanup();
    process().startEvent().components().remove(m_ossia_startEvent);
  }
  if(m_ossia_endEvent)
  {
    m_ossia_endEvent->cleanup();
    process().endEvent().components().remove(m_ossia_endEvent);
  }
  if(m_ossia_startTimeNode)
  {
    m_ossia_startTimeNode->OSSIATimeNode()->cleanup();
    m_ossia_startTimeNode->cleanup();
    process().startTimeNode().components().remove(m_ossia_startTimeNode);
  }
  if(m_ossia_endTimeNode)
  {
    m_ossia_endTimeNode->OSSIATimeNode()->cleanup();
    m_ossia_endTimeNode->cleanup();
    process().endTimeNode().components().remove(m_ossia_endTimeNode);
  }

  m_ossia_constraint = nullptr;
  m_ossia_startState = nullptr;
  m_ossia_endState = nullptr;
  m_ossia_startEvent = nullptr;
  m_ossia_endEvent = nullptr;
  m_ossia_startTimeNode = nullptr;
  m_ossia_endTimeNode = nullptr;
}

void Component::stop()
{
  ProcessComponent::stop();
  process().constraint().duration.setPlayPercentage(0);
}

void Component::startConstraintExecution(const Id<Scenario::ConstraintModel>&)
{
  m_ossia_constraint->executionStarted();
}

void Component::stopConstraintExecution(const Id<Scenario::ConstraintModel>&)
{
  m_ossia_constraint->executionStopped();
}
}
}
