#pragma once
#include <Loop/LoopProcessModel.hpp>
#include <Process/Execution/ProcessComponent.hpp>

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

namespace Execution
{
class EventComponent;
class StateComponent;
class IntervalRawPtrComponent;
class TimeSyncRawPtrComponent;
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
class Component final : public ::Execution::ProcessComponent_T<Loop::ProcessModel, ossia::loop>
{
  COMPONENT_METADATA("77b987ae-7bc8-4273-aa9c-9e4ba53a053d")
public:
  Component(
      ::Loop::ProcessModel& element,
      const ::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  virtual ~Component();

  void cleanup() override;

  void stop() override;

private:
  void startIntervalExecution(const Id<Scenario::IntervalModel>&);
  void stopIntervalExecution(const Id<Scenario::IntervalModel>&);

private:
  std::shared_ptr<Execution::IntervalRawPtrComponent> m_ossia_interval;

  std::shared_ptr<Execution::TimeSyncRawPtrComponent> m_ossia_startTimeSync;
  std::shared_ptr<Execution::TimeSyncRawPtrComponent> m_ossia_endTimeSync;

  std::shared_ptr<Execution::EventComponent> m_ossia_startEvent;
  std::shared_ptr<Execution::EventComponent> m_ossia_endEvent;

  std::shared_ptr<Execution::StateComponent> m_ossia_startState;
  std::shared_ptr<Execution::StateComponent> m_ossia_endState;
};

using ComponentFactory = ::Execution::ProcessComponentFactory_T<Component>;
}
}

SCORE_CONCRETE_COMPONENT_FACTORY(
    Execution::ProcessComponentFactory,
    Loop::RecreateOnPlay::ComponentFactory)
