// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SplineComponent.hpp"
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/score2OSSIA.hpp>
#include <ossia/dataflow/nodes/spline.hpp>
namespace Spline
{
namespace RecreateOnPlay
{
using spline = ossia::nodes::spline;
Component::Component(
    ::Spline::ProcessModel& element,
    const ::Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
  : ::Engine::Execution::
      ProcessComponent_T<Spline::ProcessModel, ossia::node_process>{
        element,
        ctx,
        id, "Executor::SplineComponent", parent}
{
  auto node = std::make_shared<spline>();
  this->node = node;
  m_ossia_process = std::make_shared<ossia::node_process>(node);

  con(element, &Spline::ProcessModel::splineChanged,
      this, [this] { this->recompute(); });

  recompute();
}

Component::~Component()
{
}

void Component::recompute()
{
  const Engine::Execution::Context& s = this->system();
  auto g = process().spline();
  s.executionQueue.enqueue(
        [proc=std::dynamic_pointer_cast<spline>(OSSIAProcess().node)
        ,g]
  {
    proc->set_spline(g);
  });
}

}
}
