// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "GradientComponent.hpp"
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/score2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <ossia/editor/automation/automation.hpp>
namespace Gradient
{
namespace RecreateOnPlay
{

Component::Component(
    ::Engine::Execution::IntervalComponent& parentInterval,
    ::Gradient::ProcessModel& element,
    const ::Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
  : ::Engine::Execution::
      ProcessComponent_T<Gradient::ProcessModel, ossia::color_automation>{
        parentInterval,
        element,
        ctx,
        id, "Executor::GradientComponent", parent}
{
  m_ossia_process = std::make_shared<ossia::color_automation>();

  con(element, &Gradient::ProcessModel::addressChanged,
      this, [this] (const auto&) { this->recompute(); });

  // TODO the tween case will reset the "running" value,
  // so it may not work perfectly.
  con(element, &Gradient::ProcessModel::tweenChanged,
      this, [this] (const auto&) { this->recompute(); });
  con(element, &Gradient::ProcessModel::gradientChanged,
      this, [this] () { this->recompute(); });

  recompute();
}

static ossia::hunter_lab to_ossia_color(const QColor& c)
{
  switch(c.spec())
  {
    case QColor::Rgb:
    {
      ossia::rgb r{(float)c.redF(), (float)c.greenF(), (float)c.blueF()};
      return ossia::hunter_lab{r};
    }
    case QColor::Hsv:
    case QColor::Cmyk:
    case QColor::Hsl:
      return to_ossia_color(c.toRgb());
    case QColor::Invalid:
    default:
      return ossia::hunter_lab{};
  }
}

static auto to_ossia_gradient(const Gradient::ProcessModel::gradient_colors& c)
{
  ossia::color_automation::grad_type g;
  for(auto& e : c)
  {
    g.insert(std::make_pair(e.first, to_ossia_color(e.second)));
  }
  return g;
}

void Component::recompute()
{
  const Engine::Execution::Context& s = this->system();
  auto dest = Engine::score_to_ossia::makeDestination(
        s.devices.list(),
        process().address());

  if (dest)
  {
    auto& d = *dest;
    auto g = process().gradient();

    s.executionQueue.enqueue(
          [proc=std::dynamic_pointer_cast<ossia::color_automation>(m_ossia_process)
          ,g
          ,d_=d]
    {
      proc->set_destination(std::move(d_));
      proc->set_gradient(to_ossia_gradient(g));
    });
    return;
  }
}

}
}
