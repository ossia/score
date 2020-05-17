#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

#include <Color/GradientModel.hpp>

#include <memory>

namespace Device
{
class DeviceList;
}

namespace Gradient
{
namespace RecreateOnPlay
{
class Component final
    : public ::Execution::ProcessComponent_T<Gradient::ProcessModel, ossia::node_process>
{
  COMPONENT_METADATA("45467316-6c07-47f9-9d68-9a9de0360402")
public:
  Component(
      Gradient::ProcessModel& element,
      const ::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);
  ~Component() override;

private:
  void recompute();
};
using ComponentFactory = ::Execution::ProcessComponentFactory_T<Component>;
}
}

SCORE_CONCRETE_COMPONENT_FACTORY(
    Execution::ProcessComponentFactory,
    Gradient::RecreateOnPlay::ComponentFactory)
