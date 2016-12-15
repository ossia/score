#pragma once
#include <Engine/Executor/ProcessElement.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <iscore/model/Identifier.hpp>
#include <memory>

class DeviceList;
namespace Process
{
class ProcessModel;
}
class QObject;
namespace Loop
{
class ProcessModel;
} // namespace Loop
namespace ossia
{
class loop;
class time_process;
} // namespace OSSIA
namespace Engine
{
namespace Execution
{
class EventElement;
class StateElement;
class TimeNodeElement;
}
}
namespace Scenario
{
class ConstraintModel;
}

namespace Loop
{
namespace RecreateOnPlay
{
// TODO see if this can be used for the base scenario model too.
class Component final : public ::Engine::Execution::
                            ProcessComponent_T<Loop::ProcessModel, ossia::loop>
{
  COMPONENT_METADATA("77b987ae-7bc8-4273-aa9c-9e4ba53a053d")
public:
  Component(
      ::Engine::Execution::ConstraintElement& parentConstraint,
      ::Loop::ProcessModel& element,
      const ::Engine::Execution::Context& ctx,
      const Id<iscore::Component>& id,
      QObject* parent);

  virtual ~Component();

  void stop() override;

private:
  void startConstraintExecution(const Id<Scenario::ConstraintModel>&);
  void stopConstraintExecution(const Id<Scenario::ConstraintModel>&);

private:
  ::Engine::Execution::ConstraintElement* m_ossia_constraint{};

  ::Engine::Execution::TimeNodeElement* m_ossia_startTimeNode{};
  ::Engine::Execution::TimeNodeElement* m_ossia_endTimeNode{};

  ::Engine::Execution::EventElement* m_ossia_startEvent{};
  ::Engine::Execution::EventElement* m_ossia_endEvent{};

  ::Engine::Execution::StateElement* m_ossia_startState{};
  ::Engine::Execution::StateElement* m_ossia_endState{};
};

using ComponentFactory
    = ::Engine::Execution::ProcessComponentFactory_T<Component>;
}
}

ISCORE_CONCRETE_COMPONENT_FACTORY(
    Engine::Execution::ProcessComponentFactory,
    Loop::RecreateOnPlay::ComponentFactory)
