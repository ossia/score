#include "Executor.hpp"

#include <Skeleton/Process.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Engine/score2OSSIA.hpp>
#include <ossia/editor/state/state_element.hpp>

namespace Skeleton
{
ProcessExecutor::ProcessExecutor(
    const Device::DeviceList& devices):
  m_devices{devices}
{
}


void ProcessExecutor::start(ossia::state&)
{
}

void ProcessExecutor::stop()
{
}

void ProcessExecutor::pause()
{
}

void ProcessExecutor::resume()
{
}

ossia::state_element ProcessExecutor::state(ossia::time_value date, double pos)
{
  State::Address address{"my_device", {"a", "banana"}};
  ossia::value value = std::abs(qrand()) % 100;
  State::Message m;
  m.address = address;
  m.value = value;

  if(auto res = Engine::score_to_ossia::message(m, m_devices))
  {
    if(unmuted())
      return *res;
    return {};
  }
  else
  {
    return {};
  }
}

ProcessExecutorComponent::ProcessExecutorComponent(
    Engine::Execution::IntervalComponent& parentInterval,
    Skeleton::Model& element,
    const Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent):
  ProcessComponent_T{
    parentInterval, element, ctx, id, "SkeletonExecutorComponent", parent}
{
  m_ossia_process = std::make_shared<ProcessExecutor>(ctx.devices.list());
}

}
