#pragma once
#include <ossia/network/value/value.hpp>
#include <Automation/AutomationModel.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <memory>
namespace ossia
{
class curve_abstract;
}

namespace Device
{
class DeviceList;
}

namespace Automation
{
namespace RecreateOnPlay
{
class Component final
    : public ::Engine::Execution::
          ProcessComponent_T<Automation::ProcessModel, ossia::node_process>
{
  COMPONENT_METADATA("f759eacd-5a67-4627-bbe8-c649e0f9b6c5")
public:
  Component(
      Automation::ProcessModel& element,
      const ::Engine::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  ~Component();
private:
  void recompute();

  std::shared_ptr<ossia::curve_abstract>
  on_curveChanged(ossia::val_type, const optional<ossia::destination>&);

  template <typename T>
  std::shared_ptr<ossia::curve_abstract>
  on_curveChanged_impl(const optional<ossia::destination>&);
  ossia::node_ptr m_node;
};
using ComponentFactory
    = ::Engine::Execution::ProcessComponentFactory_T<Component>;
}
}


SCORE_CONCRETE_COMPONENT_FACTORY(
    Engine::Execution::ProcessComponentFactory,
    Automation::RecreateOnPlay::ComponentFactory)
