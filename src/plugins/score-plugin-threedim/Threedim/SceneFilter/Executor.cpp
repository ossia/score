#include "Executor.hpp"

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/Graph/SceneFilterNode.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>
#include <Threedim/SceneFilter/Process.hpp>

#include <ossia/dataflow/port.hpp>

#include <score/document/DocumentContext.hpp>

namespace Gfx::SceneFilter
{
class scene_filter_exec_node final : public gfx_exec_node
{
public:
  scene_filter_exec_node(GfxExecutionAction& ctx)
      : gfx_exec_node{ctx}
  {
  }

  void init()
  {
    auto node = std::make_unique<score::gfx::SceneFilterNode>();
    id = exec_context->ui->register_node(std::move(node));
  }

  ~scene_filter_exec_node() { exec_context->ui->unregister_node(id); }

  std::string label() const noexcept override { return "Gfx::SceneFilter_node"; }
};

ProcessExecutorComponent::ProcessExecutorComponent(
    Gfx::SceneFilter::Model& element,
    const Execution::Context& ctx,
    QObject* parent)
    : ProcessComponent_T{element, ctx, "sceneFilterComponent", parent}
{
  auto n = ossia::make_node<scene_filter_exec_node>(
      *ctx.execState, ctx.doc.plugin<DocumentPlugin>().exec);

  n->add_geometry();
  {
    auto ctrl = qobject_cast<Process::ControlInlet*>(element.inlets()[1]);
    auto& p = n->add_control();
    ctrl->setupExecution(*n->root_inputs().back(), this);
    p->value = ctrl->value();
    QObject::connect(
        ctrl, &Process::ControlInlet::valueChanged, this,
        con_unvalidated{ctx, 1, 0, n});
  }
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
