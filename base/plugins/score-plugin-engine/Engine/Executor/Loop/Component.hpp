#pragma once
#include <Engine/Executor/ProcessComponent.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <score/model/Identifier.hpp>
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
class TimeSyncComponent;
}
}
namespace Scenario
{
class IntervalModel;
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
      ::Engine::Execution::IntervalComponent& parentInterval,
      ::Loop::ProcessModel& element,
      const ::Engine::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  virtual ~Component();

  void cleanup() override;

  void stop() override;

private:
  void startIntervalExecution(const Id<Scenario::IntervalModel>&);
  void stopIntervalExecution(const Id<Scenario::IntervalModel>&);

private:
  Engine::Execution::IntervalComponent* m_ossia_interval{};

  Engine::Execution::TimeSyncComponent* m_ossia_startTimeSync{};
  Engine::Execution::TimeSyncComponent* m_ossia_endTimeSync{};

  Engine::Execution::EventComponent* m_ossia_startEvent{};
  Engine::Execution::EventComponent* m_ossia_endEvent{};

  Engine::Execution::StateComponent* m_ossia_startState{};
  Engine::Execution::StateComponent* m_ossia_endState{};
};

using ComponentFactory
    = ::Engine::Execution::ProcessComponentFactory_T<Component>;
}
}

SCORE_CONCRETE_COMPONENT_FACTORY(
    Engine::Execution::ProcessComponentFactory,
    Loop::RecreateOnPlay::ComponentFactory)
