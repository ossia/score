#pragma once
#include <ossia/editor/automation/automation.hpp>
#include <ossia/editor/value/value.hpp>
#include <Automation/Spline/SplineAutomModel.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <ossia/editor/scenario/time_constraint.hpp>
#include <ossia/editor/curve/curve_segment/easing.hpp>
#include <memory>
#include <tinysplinecpp.h>
namespace ossia
{
class curve_abstract;

class spline_automation : public ossia::time_process
{
    tinyspline::BSpline m_spline;
  public:
    spline_automation()
    {

    }

    void set_destination(ossia::Destination a)
    {
      m_address = std::move(a);
    }

    void set_spline(const Spline::spline_data& t);

    ossia::state_element offset(ossia::time_value) override
    {
      return {};
    }

    ossia::state_element state() override;

    void start() override
    {
    }
    void stop() override
    {
    }
    void pause() override
    {
    }
    void resume() override
    {
    }

  private:
    optional<ossia::Destination> m_address;
    Spline::spline_data m_data;
};
}

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
        ::Engine::Execution::ConstraintComponent& parentConstraint,
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
