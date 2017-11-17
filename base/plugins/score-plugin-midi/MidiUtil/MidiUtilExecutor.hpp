#pragma once
#include <ossia/editor/scenario/time_process.hpp>
#include <ossia/editor/state/state_element.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <boost/container/flat_set.hpp>
#include <ossia/dataflow/node_process.hpp>
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
namespace MidiUtil
{
class ProcessModel;
namespace Executor
{

class Component final
    : public ::Engine::Execution::
          ProcessComponent_T<MidiUtil::ProcessModel, ossia::node_process>
{
  COMPONENT_METADATA("151518d7-a5e9-46b1-9125-f0b23749d222")
public:
    static const constexpr bool is_unique = true;
  Component(
      MidiUtil::ProcessModel& element,
      const Engine::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);
  ~Component() override;

  private:
  ossia::node_ptr m_node;
};

using ComponentFactory
    = ::Engine::Execution::ProcessComponentFactory_T<Component>;
}
}
