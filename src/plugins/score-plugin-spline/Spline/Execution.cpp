// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Execution.hpp"

#include <Process/ExecutionContext.hpp>

#include <score/tools/Bind.hpp>

#include <ossia/dataflow/nodes/spline.hpp>
namespace Spline
{
namespace RecreateOnPlay
{
using spline = ossia::nodes::spline;
Component::Component(
    ::Spline::ProcessModel& element,
    const ::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : ::Execution::ProcessComponent_T<Spline::ProcessModel, ossia::node_process>{
        element,
        ctx,
        id,
        "Executor::Component",
        parent}
{
  auto node = std::make_shared<spline>();
  this->node = node;
  m_ossia_process = std::make_shared<ossia::node_process>(node);

  con(element, &Spline::ProcessModel::splineChanged, this, [this] { this->recompute(); });

  recompute();
}

Component::~Component() { }

void Component::recompute()
{
  in_exec([proc = std::dynamic_pointer_cast<spline>(OSSIAProcess().node), g = process().spline()] {
    proc->set_spline(g);
  });
}
}
}
