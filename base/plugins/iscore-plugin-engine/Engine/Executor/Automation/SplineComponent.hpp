#pragma once
#include <ossia/editor/automation/automation.hpp>
#include <ossia/editor/value/value.hpp>
#include <Automation/Spline/SplineAutomModel.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/curve/curve_segment/easing.hpp>
#include <ossia/editor/automation/spline.hpp>
#include <memory>

namespace Device
{
class DeviceList;
}

namespace Spline
{
namespace RecreateOnPlay
{
class Component final
    : public ::Engine::Execution::
    ProcessComponent_T<Spline::ProcessModel, ossia::spline_automation>
{
    COMPONENT_METADATA("553f99e0-a12a-4a1a-a070-63f198908c6d")
    public:
      Component(
        ::Engine::Execution::IntervalComponent& parentInterval,
        Spline::ProcessModel& element,
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
    Spline::RecreateOnPlay::ComponentFactory)
