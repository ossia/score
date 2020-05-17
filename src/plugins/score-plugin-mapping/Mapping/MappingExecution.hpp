#pragma once
#include <Mapping/MappingModel.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <State/Address.hpp>

#include <ossia/dataflow/node_process.hpp>
#include <ossia/network/value/value.hpp>

namespace ossia
{
class curve_abstract;
}
namespace Device
{
class DeviceList;
}

namespace Mapping
{
namespace RecreateOnPlay
{
class Component final
    : public ::Execution::ProcessComponent_T<Mapping::ProcessModel, ossia::node_process>
{
  COMPONENT_METADATA("da360b58-9885-4106-be54-8e272ed45dbe")
public:
  Component(
      ::Mapping::ProcessModel& element,
      const ::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  ~Component();

private:
  void recompute();
  std::shared_ptr<ossia::curve_abstract>
  rebuildCurve(ossia::val_type source, ossia::val_type target);

  template <typename T>
  std::shared_ptr<ossia::curve_abstract> on_curveChanged_impl(ossia::val_type target);

  template <typename X_T, typename Y_T>
  std::shared_ptr<ossia::curve_abstract> on_curveChanged_impl2();
};

using ComponentFactory = ::Execution::ProcessComponentFactory_T<Component>;
}
}

SCORE_CONCRETE_COMPONENT_FACTORY(
    Execution::ProcessComponentFactory,
    Mapping::RecreateOnPlay::ComponentFactory)
