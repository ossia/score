// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SplineComponent.hpp"
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <ossia/editor/automation/automation.hpp>
namespace Spline
{
namespace RecreateOnPlay
{

Component::Component(
    ::Engine::Execution::ConstraintComponent& parentConstraint,
    ::Spline::ProcessModel& element,
    const ::Engine::Execution::Context& ctx,
    const Id<iscore::Component>& id,
    QObject* parent)
  : ::Engine::Execution::
      ProcessComponent_T<Spline::ProcessModel, ossia::spline_automation>{
        parentConstraint,
        element,
        ctx,
        id, "Executor::SplineComponent", parent}
{
  m_ossia_process = std::make_shared<ossia::spline_automation>();

  con(element, &Spline::ProcessModel::addressChanged,
      this, [this] (const auto&) { this->recompute(); });

  // TODO the tween case will reset the "running" value,
  // so it may not work perfectly.
  con(element, &Spline::ProcessModel::tweenChanged,
      this, [this] (const auto&) { this->recompute(); });
  con(element, &Spline::ProcessModel::splineChanged,
      this, [this] () { this->recompute(); });

  recompute();
}

void Component::recompute()
{
  const Engine::Execution::Context& s = this->system();
  auto dest = Engine::iscore_to_ossia::makeDestination(
        s.devices.list(),
        process().address());

  if (dest)
  {
    auto& d = *dest;
    auto g = process().spline();

    s.executionQueue.enqueue(
          [proc=std::dynamic_pointer_cast<ossia::spline_automation>(m_ossia_process)
          ,g
          ,d_=d]
    {
      proc->set_destination(std::move(d_));
      proc->set_spline(g);
    });
  }
}

}
}

