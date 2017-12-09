#pragma once
#include <Automation/Color/GradientAutomModel.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <memory>
#include <ossia/dataflow/node_process.hpp>

namespace Device
{
class DeviceList;
}

namespace Gradient
{
namespace RecreateOnPlay
{
class Component final
    : public ::Engine::Execution::
    ProcessComponent_T<Gradient::ProcessModel, ossia::node_process>
{
    COMPONENT_METADATA("45467316-6c07-47f9-9d68-9a9de0360402")
    public:
      Component(
        Gradient::ProcessModel& element,
        const ::Engine::Execution::Context& ctx,
        const Id<score::Component>& id,
        QObject* parent);
      ~Component();

  private:
    void recompute();

};
using ComponentFactory
= ::Engine::Execution::ProcessComponentFactory_T<Component>;
}
}


SCORE_CONCRETE_COMPONENT_FACTORY(
    Engine::Execution::ProcessComponentFactory,
    Gradient::RecreateOnPlay::ComponentFactory)
