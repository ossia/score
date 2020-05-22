// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "GradientExecution.hpp"

#include <Process/ExecutionContext.hpp>

#include <score/tools/Bind.hpp>

#include <ossia/dataflow/nodes/gradient.hpp>
namespace Gradient
{
namespace RecreateOnPlay
{
using gradient = ossia::nodes::gradient;
Component::Component(
    ::Gradient::ProcessModel& element,
    const ::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : ::Execution::ProcessComponent_T<Gradient::ProcessModel, ossia::node_process>{
        element,
        ctx,
        id,
        "Executor::GradientComponent",
        parent}
{
  auto node = std::make_shared<gradient>();
  this->node = node;
  m_ossia_process = std::make_shared<ossia::nodes::gradient_process>(node);

  con(*element.outlet, &Process::Port::addressChanged, this, [=](const auto&) {
    this->in_exec([node] { node->tween = std::nullopt; });
  });

  // TODO the tween case will reset the "running" value,
  // so it may not work perfectly.
  con(element, &Gradient::ProcessModel::tweenChanged, this, [=](bool b) {
    this->in_exec([=] {
      node->tween = std::nullopt;
      node->mustTween = b;
    });
  });
  con(element, &Gradient::ProcessModel::gradientChanged, this, [this] { this->recompute(); });

  recompute();
}

Component::~Component() { }

static ossia::hunter_lab to_ossia_color(QColor c)
{
  switch (c.spec())
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
  gradient::grad_type g;
  for (auto& e : c)
  {
    g.insert(std::make_pair(e.first, to_ossia_color(e.second)));
  }
  return g;
}

void Component::recompute()
{
  const Execution::Context& s = this->system();
  auto g = process().gradient();

  s.executionQueue.enqueue([proc = std::dynamic_pointer_cast<gradient>(OSSIAProcess().node), g] {
    proc->set_gradient(to_ossia_gradient(g));
  });
}
}
}
