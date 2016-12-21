#include <ossia/editor/scenario/time_event.hpp>
#include <ossia/editor/scenario/time_node.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
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
    ::Engine::Execution::ConstraintElement& parentConstraint,
    ::Loop::ProcessModel& element,
    const ::Engine::Execution::Context& ctx,
    const Id<iscore::Component>& id,
    QObject* parent)
    : ::Engine::Execution::ProcessComponent_T<Loop::ProcessModel, ossia::loop>{
          parentConstraint, element, ctx, id, "LoopComponent", parent}
{
  ossia::time_value main_duration(Engine::iscore_to_ossia::time(
      element.constraint().duration.defaultDuration()));

  auto loop = new ossia::loop(
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
  auto startTN = loop->getPatternStartTimeNode();
  auto endTN = loop->getPatternEndTimeNode();
  auto startEV = *startTN->timeEvents().begin();
  auto endEV = *endTN->timeEvents().begin();

  using namespace Engine::Execution;
  m_ossia_startTimeNode = new TimeNodeElement{startTN, element.startTimeNode(),
                                              system().devices.list(), this};
  m_ossia_endTimeNode = new TimeNodeElement{endTN, element.endTimeNode(),
                                            system().devices.list(), this};

  m_ossia_startEvent = new EventElement{startEV, element.startEvent(),
                                        system().devices.list(), this};
  m_ossia_endEvent = new EventElement{endEV, element.endEvent(),
                                      system().devices.list(), this};

  m_ossia_startState
      = new StateElement{element.startState(), *startEV, system(), this};
  m_ossia_endState
      = new StateElement{element.endState(), *endEV, system(), this};

  m_ossia_constraint = new ConstraintElement{
      loop->getPatternTimeConstraint(), element.constraint(), system(), this};
}

Component::~Component()
{
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
