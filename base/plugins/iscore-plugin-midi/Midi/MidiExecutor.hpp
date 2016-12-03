#pragma once
#include <ossia/editor/scenario/time_process.hpp>
#include <ossia/editor/state/state_element.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Executor/ProcessElement.hpp>
#include <set>

namespace ossia
{
namespace net
{
namespace midi
{
class channel_node;
}
}
}

namespace Device
{
class DeviceList;
}
namespace Midi
{
class ProcessModel;
namespace Executor
{
class ProcessExecutor final : public ossia::time_process
{
public:
  ProcessExecutor(
      const Midi::ProcessModel& proc, const Device::DeviceList& devices);
  ~ProcessExecutor();

  ossia::state_element state(double);
  ossia::state_element state() override;
  ossia::state_element offset(ossia::time_value) override;

  void stop() override;

private:
  const Midi::ProcessModel& m_process;

  ossia::net::midi::channel_node* m_channelNode{};
  ossia::state m_lastState;

  std::set<int> m_playing;
};

class Component final
    : public ::Engine::Execution::
          ProcessComponent_T<Midi::ProcessModel, ProcessExecutor>
{
  COMPONENT_METADATA("6d5334a5-7b8c-45df-9805-11d1b4472cdf")
public:
  Component(
      Engine::Execution::ConstraintElement& parentConstraint,
      Midi::ProcessModel& element,
      const Engine::Execution::Context& ctx,
      const Id<iscore::Component>& id,
      QObject* parent);
};

using ComponentFactory
    = ::Engine::Execution::ProcessComponentFactory_T<Component>;
}
}
