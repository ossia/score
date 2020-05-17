#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

#include <Spline/SplineModel.hpp>

namespace Device
{
class DeviceList;
}

namespace Spline
{
namespace RecreateOnPlay
{
class Component final
    : public ::Execution::ProcessComponent_T<Spline::ProcessModel, ossia::node_process>
{
  COMPONENT_METADATA("6b5b4706-6ae7-46ab-b06a-bece7e03e6f7")
public:
  Component(
      Spline::ProcessModel& element,
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
    Spline::RecreateOnPlay::ComponentFactory)
