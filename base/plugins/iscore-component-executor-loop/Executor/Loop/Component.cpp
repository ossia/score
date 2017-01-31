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
  ossia::time_value main_duration(Engine::iscore_to_ossia::time(
      element.constraint().duration.defaultDuration()));

  auto loop = std::make_shared<ossia::loop>(
      main_duration,
      [](ossia::time_value, ossia::time_value, const ossia::state_element&) {},
      [this, &element](ossia::time_event::Status newStatus) {

        element.startEvent().setStatus(
            static_cast<Scenario::ExecutionStatus>(newStatus));
        switch (newStatus)
        {
          case ossia::time_event::Status::NONE:
            break;
          case ossia::time_event::Status::PENDING:
            break;
          case ossia::time_event::Status::HAPPENED:
            startConstraintExecution(
                m_ossia_constraint->iscoreConstraint().id());
            break;
          case ossia::time_event::Status::DISPOSED:
            break;
          default:
            ISCORE_TODO;
            break;
        }
      },
      [this, &element](ossia::time_event::Status newStatus) {

        element.endEvent().setStatus(
            static_cast<Scenario::ExecutionStatus>(newStatus));
        switch (newStatus)
        {
          case ossia::time_event::Status::NONE:
            break;
          case ossia::time_event::Status::PENDING:
            break;
          case ossia::time_event::Status::HAPPENED:
            stopConstraintExecution(
                m_ossia_constraint->iscoreConstraint().id());
            break;
          case ossia::time_event::Status::DISPOSED:
            break;
          default:
            ISCORE_TODO;
            break;
        }

      });

  m_ossia_process = loop;

  // TODO also states in BasEelement
  // TODO put graphical settings somewhere.
  auto main_start_node = loop->getPatternStartTimeNode();
  auto main_end_node = loop->getPatternEndTimeNode();
  auto main_start_event = *main_start_node->timeEvents().begin();
  auto main_end_event = *main_end_node->timeEvents().begin();

  using namespace Engine::Execution;
  m_ossia_startTimeNode = std::make_shared<TimeNodeComponent>(element.startTimeNode(),
                                              system(), iscore::newId(element.startTimeNode()), this);
  m_ossia_endTimeNode = std::make_shared<TimeNodeComponent>(element.endTimeNode(),
                                            system(), iscore::newId(element.endTimeNode()), this);

  m_ossia_startEvent = std::make_shared<EventComponent>(element.startEvent(),
                                        system(), iscore::newId(element.startEvent()), this);
  m_ossia_endEvent = std::make_shared<EventComponent>(element.endEvent(),
                                      system(), iscore::newId(element.endEvent()), this);

  m_ossia_startState
      = std::make_shared<StateComponent>(element.startState(), system(), iscore::newId(element.startState()), this);
  m_ossia_endState
      = std::make_shared<StateComponent>(element.endState(), system(), iscore::newId(element.endState()), this);


  m_ossia_constraint = std::make_shared<ConstraintComponent>(element.constraint(), system(), iscore::newId(element.constraint()), this);

  m_ossia_startTimeNode->onSetup(main_start_node, m_ossia_startTimeNode->makeTrigger());
  m_ossia_endTimeNode->onSetup(main_end_node, m_ossia_endTimeNode->makeTrigger());
  m_ossia_startEvent->onSetup(main_start_event, m_ossia_startEvent->makeExpression(), (ossia::time_event::OffsetBehavior)element.startEvent().offsetBehavior());
  m_ossia_endEvent->onSetup(main_end_event, m_ossia_endEvent->makeExpression(), (ossia::time_event::OffsetBehavior)element.endEvent().offsetBehavior());
  m_ossia_startState->onSetup(main_start_event);
  m_ossia_endState->onSetup(main_end_event);
  m_ossia_constraint->onSetup(loop->getPatternTimeConstraint(), m_ossia_constraint->makeDurations(), false);
}

Component::~Component()
{
}

void Component::cleanup()
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
