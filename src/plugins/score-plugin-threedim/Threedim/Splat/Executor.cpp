#include "Executor.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/TexturePort.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/dataflow/port.hpp>

#include <Threedim/Splat/GaussianSplatNode.hpp>
#include <Threedim/Splat/Process.hpp>

namespace Gfx::Splat
{
class model_display_node final : public gfx_exec_node
{
public:
  model_display_node(GfxExecutionAction& ctx)
      : gfx_exec_node{ctx}
  {
  }

  void init()
  {
    auto node = std::make_unique<score::gfx::GaussianSplatNode>();
    id = exec_context->ui->register_node(std::move(node));
  }

  ~model_display_node() { exec_context->ui->unregister_node(id); }

  std::string label() const noexcept override { return "Gfx::Splat_node"; }
};

ProcessExecutorComponent::ProcessExecutorComponent(
    Gfx::Splat::Model& element, const Execution::Context& ctx, QObject* parent)
    : ProcessComponent_T{element, ctx, "modelComponent", parent}
{
  auto n = ossia::make_node<model_display_node>(
      *ctx.execState, ctx.doc.plugin<DocumentPlugin>().exec);

  for(auto* outlet : element.outlets())
  {
    if(auto out = qobject_cast<Gfx::TextureOutlet*>(outlet))
    {
      out->nodeId = n->id;
    }
  }
  // Buffer input (port 0)
  element.inlets()[0]->setupExecution(*n->add_texture(), this);

  // Camera controls: Position(1), Center(2), FOV(3), Near(4), Far(5)
  for(std::size_t i = 1; i <= 9; i++)
  {
    auto ctrl = qobject_cast<Process::ControlInlet*>(element.inlets()[i]);
    auto& p = n->add_control();
    ctrl->setupExecution(*n->root_inputs().back(), this);
    p->value = ctrl->value();

    QObject::connect(
        ctrl, &Process::ControlInlet::valueChanged, this,
        con_unvalidated{ctx, i - 1, 0, n});
  }

  n->add_texture_out();

  n->init();
  this->node = n;
  m_ossia_process = std::make_shared<ossia::node_process>(n);
}

void ProcessExecutorComponent::cleanup()
{
  for(auto* outlet : this->process().outlets())
  {
    if(auto out = qobject_cast<TextureOutlet*>(outlet))
    {
      out->nodeId = -1;
    }
  }
  ProcessComponent_T::cleanup();
}
}
