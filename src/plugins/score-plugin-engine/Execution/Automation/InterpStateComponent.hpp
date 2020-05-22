#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>
#include <ossia/network/value/destination.hpp>
#include <ossia/network/value/value.hpp>

#include <InterpState/InterpStateProcess.hpp>

#include <memory>
namespace ossia
{
class curve_abstract;
}

namespace Device
{
class DeviceList;
}

namespace InterpState
{
class ExecComponent final
    : public ::Execution::ProcessComponent_T<InterpState::ProcessModel, ossia::node_process>
{
  COMPONENT_METADATA("66327ccc-1478-4bef-9ce7-3c9765bd76a7")
public:
  ExecComponent(
      InterpState::ProcessModel& element,
      const Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  ~ExecComponent() override;

private:
  void recompute();

  std::shared_ptr<ossia::curve_abstract>
  on_curveChanged(ossia::val_type, const std::optional<ossia::destination>&);

  template <typename T>
  std::shared_ptr<ossia::curve_abstract> on_curveChanged_impl(const std::optional<ossia::destination>&);
};
using ExecComponentFactory = ::Execution::ProcessComponentFactory_T<ExecComponent>;
}

SCORE_CONCRETE_COMPONENT_FACTORY(
    Execution::ProcessComponentFactory,
    InterpState::ExecComponentFactory)
