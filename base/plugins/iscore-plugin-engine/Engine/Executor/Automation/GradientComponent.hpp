#pragma once
#include <ossia/editor/automation/automation.hpp>
#include <ossia/editor/value/value.hpp>
#include <Automation/Color/GradientAutomModel.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <ossia/editor/scenario/time_constraint.hpp>
#include <ossia/editor/curve/curve_segment/easing.hpp>
#include <ossia/editor/automation/gradient.hpp>
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
    : public ::Engine::Execution::
    ProcessComponent_T<Gradient::ProcessModel, ossia::color_automation>
{
    COMPONENT_METADATA("45467316-6c07-47f9-9d68-9a9de0360402")
    public:
      Component(
        ::Engine::Execution::ConstraintComponent& parentConstraint,
        Gradient::ProcessModel& element,
        const ::Engine::Execution::Context& ctx,
        const Id<iscore::Component>& id,
        QObject* parent);

  private:
    void recompute();

};
using ComponentFactory
= ::Engine::Execution::ProcessComponentFactory_T<Component>;
}
}


ISCORE_CONCRETE_COMPONENT_FACTORY(
    Engine::Execution::ProcessComponentFactory,
    Gradient::RecreateOnPlay::ComponentFactory)
