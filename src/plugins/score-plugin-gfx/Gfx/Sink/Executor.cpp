#include "Executor.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/ExecutionContext.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/Graph/SinkNode.hpp>
#include <Gfx/Sink/Process.hpp>
#include <Gfx/TexturePort.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/network/value/value_conversion.hpp>

namespace Gfx::Sink
{
class sink_node final : public gfx_exec_node
{
public:
  sink_node(GfxExecutionAction& ctx, double rate)
      : gfx_exec_node{ctx}
  {
    id = exec_context->ui->register_node(std::make_unique<score::gfx::SinkNode>(rate));
  }

  ~sink_node() { exec_context->ui->unregister_node(id); }

  std::string label() const noexcept override { return "Gfx::sink_node"; }
};

ProcessExecutorComponent::ProcessExecutorComponent(
    Gfx::Sink::Model& element, const Execution::Context& ctx, QObject* parent)
    : ProcessComponent_T{element, ctx, "sinkComponent", parent}
{
  auto rate = qobject_cast<Process::ControlInlet*>(element.inlets()[1]);
  auto n = ossia::make_node<sink_node>(
      *ctx.execState, ctx.doc.plugin<DocumentPlugin>().exec,
      ossia::convert<float>(rate->value()));

  // Inlet 0: the texture; inlet 1: the rate.
  auto tex = n->add_texture();
  if(auto in = qobject_cast<Gfx::TextureInlet*>(element.inlets()[0]))
    in->setupExecution(*tex, this);
  auto& p = n->add_control();
  p->value = rate->value();
  rate->setupExecution(*n->root_inputs()[1], this);
  QObject::connect(
      rate, &Process::ControlInlet::valueChanged, this, con_unvalidated{ctx, 0, 0, n});

  this->node = n;
  m_ossia_process = std::make_shared<ossia::node_process>(n);
}
}
