#include "Executor.hpp"

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/Graph/FlattenedSceneFilterNode.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>
#include <Threedim/FlattenedSceneFilter/Process.hpp>

#include <ossia/dataflow/port.hpp>

#include <score/document/DocumentContext.hpp>

namespace Gfx::FlattenedSceneFilter
{
class flattened_scene_filter_exec_node final : public gfx_exec_node
{
public:
  flattened_scene_filter_exec_node(GfxExecutionAction& ctx)
      : gfx_exec_node{ctx}
  {
  }

  void init()
  {
    auto node = std::make_unique<score::gfx::FlattenedSceneFilterNode>();
    id = exec_context->ui->register_node(std::move(node));
  }

  ~flattened_scene_filter_exec_node()
  {
    exec_context->ui->unregister_node(id);
  }

  std::string label() const noexcept override
  {
    return "Gfx::FlattenedSceneFilter_node";
  }
};

ProcessExecutorComponent::ProcessExecutorComponent(
    Gfx::FlattenedSceneFilter::Model& element,
    const Execution::Context& ctx,
    QObject* parent)
    : ProcessComponent_T{element, ctx, "flattenedSceneFilterComponent", parent}
{
  auto n = ossia::make_node<flattened_scene_filter_exec_node>(
      *ctx.execState, ctx.doc.plugin<DocumentPlugin>().exec);

  // Port 0: geometry input
  n->add_geometry();

  // Ports 1-3: Mode + Match (int) + Match (string) controls
  for(std::size_t i = 1; i <= 3; i++)
  {
    auto ctrl = qobject_cast<Process::ControlInlet*>(element.inlets()[i]);
    auto& p = n->add_control();
    ctrl->setupExecution(*n->root_inputs().back(), this);
    p->value = ctrl->value();
    QObject::connect(
        ctrl,
        &Process::ControlInlet::valueChanged,
        this,
        con_unvalidated{ctx, i, 0, n});
  }

  // Port 0: geometry output
  n->add_geometry_out();

  n->init();

  this->node = n;
  m_ossia_process = std::make_shared<ossia::node_process>(n);
}

void ProcessExecutorComponent::cleanup()
{
  ProcessComponent_T::cleanup();
}
}
