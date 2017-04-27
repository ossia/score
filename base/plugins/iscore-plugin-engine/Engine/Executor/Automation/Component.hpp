#pragma once
#include <ossia/editor/automation/automation.hpp>
#include <ossia/editor/value/value.hpp>
#include <Automation/AutomationModel.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
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
          ProcessComponent_T<Automation::ProcessModel, ossia::automation>
{
  COMPONENT_METADATA("f759eacd-5a67-4627-bbe8-c649e0f9b6c5")
public:
  Component(
      ::Engine::Execution::ConstraintComponent& parentConstraint,
      Automation::ProcessModel& element,
      const ::Engine::Execution::Context& ctx,
      const Id<iscore::Component>& id,
      QObject* parent);

private:
  void recompute();

  std::shared_ptr<ossia::curve_abstract>
  on_curveChanged(ossia::val_type, const optional<ossia::Destination>&);

  template <typename T>
  std::shared_ptr<ossia::curve_abstract>
  on_curveChanged_impl(const optional<ossia::Destination>&);
};
using ComponentFactory
    = ::Engine::Execution::ProcessComponentFactory_T<Component>;
}
}

ISCORE_CONCRETE_COMPONENT_FACTORY(
    Engine::Execution::ProcessComponentFactory,
    Automation::RecreateOnPlay::ComponentFactory)
