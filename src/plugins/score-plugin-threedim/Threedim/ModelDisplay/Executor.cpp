#include "Executor.hpp"

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/TexturePort.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>
#include <Threedim/ModelDisplay/ModelDisplayNode.hpp>
#include <Threedim/ModelDisplay/Process.hpp>
#include <ossia/dataflow/port.hpp>
#include <score/document/DocumentContext.hpp>

namespace Gfx::ModelDisplay
{
class model_display_node final : public gfx_exec_node
{
public:
  model_display_node(GfxExecutionAction& ctx)
      : gfx_exec_node{ctx}
  {
    id = exec_context->ui->register_node(
        std::make_unique<score::gfx::ModelDisplayNode>());
  }

  ~model_display_node()
  {
    if (id >= 0)
      exec_context->ui->unregister_node(id);
  }

  std::string label() const noexcept override { return "Gfx::ModelDisplay_node"; }
};

ProcessExecutorComponent::ProcessExecutorComponent(
    Gfx::ModelDisplay::Model& element,
    const Execution::Context& ctx,
    QObject* parent)
    : ProcessComponent_T{element, ctx, "modelComponent", parent}
{
  auto n = ossia::make_node<model_display_node>(
      *ctx.execState, ctx.doc.plugin<DocumentPlugin>().exec);

  for (auto* outlet : element.outlets())
  {
    if (auto out = qobject_cast<Gfx::TextureOutlet*>(outlet))
    {
      out->nodeId = n->id;
    }
  }

  n->root_inputs().push_back(new ossia::texture_inlet);
  n->root_inputs().push_back(new ossia::geometry_inlet);

  for (std::size_t i = 2; i <= 8; i++)
  {
    auto ctrl = qobject_cast<Process::ControlInlet*>(element.inlets()[i]);
    auto& p = n->add_control();
    p->value = ctrl->value();

    QObject::connect(
        ctrl,
        &Process::ControlInlet::valueChanged,
        this,
        con_unvalidated{ctx, i - 2, 0, n});
  }

  n->root_outputs().push_back(new ossia::texture_outlet);

  this->node = n;
  m_ossia_process = std::make_shared<ossia::node_process>(n);
}

void ProcessExecutorComponent::cleanup()
{
  for (auto* outlet : this->process().outlets())
  {
    if (auto out = qobject_cast<TextureOutlet*>(outlet))
    {
      out->nodeId = -1;
    }
  }
}
}
