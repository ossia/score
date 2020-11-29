#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

#include <Spline3D/Model.hpp>

namespace Device
{
class DeviceList;
}

namespace Spline3D
{
namespace RecreateOnPlay
{
class Component final
    : public ::Execution::ProcessComponent_T<Spline3D::ProcessModel, ossia::node_process>
{
  COMPONENT_METADATA("438137a8-e551-4e82-9f8b-0d0a47f8a676")
public:
  Component(
      Spline3D::ProcessModel& element,
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
    Spline3D::RecreateOnPlay::ComponentFactory)
