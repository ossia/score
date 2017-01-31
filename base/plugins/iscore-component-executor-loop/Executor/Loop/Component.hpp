#pragma once
#include <Engine/Executor/ProcessComponent.hpp>
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
class EventComponent;
class StateComponent;
class TimeNodeComponent;
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
      ::Engine::Execution::ConstraintComponent& parentConstraint,
      ::Loop::ProcessModel& element,
      const ::Engine::Execution::Context& ctx,
      const Id<iscore::Component>& id,
      QObject* parent);

  virtual ~Component();

  void cleanup() override;

  void stop() override;

private:
  void startConstraintExecution(const Id<Scenario::ConstraintModel>&);
  void stopConstraintExecution(const Id<Scenario::ConstraintModel>&);

private:
  std::shared_ptr<Engine::Execution::ConstraintComponent> m_ossia_constraint;

  std::shared_ptr<Engine::Execution::TimeNodeComponent> m_ossia_startTimeNode;
  std::shared_ptr<Engine::Execution::TimeNodeComponent> m_ossia_endTimeNode;

  std::shared_ptr<Engine::Execution::EventComponent> m_ossia_startEvent;
  std::shared_ptr<Engine::Execution::EventComponent> m_ossia_endEvent;

  std::shared_ptr<Engine::Execution::StateComponent> m_ossia_startState;
  std::shared_ptr<Engine::Execution::StateComponent> m_ossia_endState;
};

using ComponentFactory
    = ::Engine::Execution::ProcessComponentFactory_T<Component>;
}
}

ISCORE_CONCRETE_COMPONENT_FACTORY(
    Engine::Execution::ProcessComponentFactory,
    Loop::RecreateOnPlay::ComponentFactory)
