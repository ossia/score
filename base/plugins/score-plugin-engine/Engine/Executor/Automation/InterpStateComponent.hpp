#pragma once
#include <ossia/dataflow/node_process.hpp>
#include <ossia/network/value/value.hpp>

#include <InterpState/InterpStateProcess.hpp>
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

namespace InterpState
{
class ExecComponent final
    : public ::Engine::Execution::
          ProcessComponent_T<InterpState::ProcessModel, ossia::node_process>
{
  COMPONENT_METADATA("66327ccc-1478-4bef-9ce7-3c9765bd76a7")
public:
  ExecComponent(
      InterpState::ProcessModel& element,
      const Engine::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  ~ExecComponent() override;

private:
  void recompute();

  std::shared_ptr<ossia::curve_abstract>
  on_curveChanged(ossia::val_type, const optional<ossia::destination>&);

  template <typename T>
  std::shared_ptr<ossia::curve_abstract>
  on_curveChanged_impl(const optional<ossia::destination>&);
};
using ExecComponentFactory
    = ::Engine::Execution::ProcessComponentFactory_T<ExecComponent>;
}

SCORE_CONCRETE_COMPONENT_FACTORY(
    Engine::Execution::ProcessComponentFactory,
    InterpState::ExecComponentFactory)
