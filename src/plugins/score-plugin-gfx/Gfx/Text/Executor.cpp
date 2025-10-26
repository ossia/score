#include "Executor.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/Graph/TextNode.hpp>
#include <Gfx/Text/Process.hpp>
#include <Gfx/TexturePort.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/dataflow/port.hpp>

namespace Gfx::Text
{
class text_node final : public gfx_exec_node
{
public:
  explicit text_node(GfxExecutionAction& ctx)
      : gfx_exec_node{ctx}
  {
    id = exec_context->ui->register_node(std::make_unique<score::gfx::TextNode>());
  }

  ~text_node()
  {
    if(id >= 0)
      exec_context->ui->unregister_node(id);
  }

  std::string label() const noexcept override { return "Gfx::text_node"; }
};

ProcessExecutorComponent::ProcessExecutorComponent(
    Gfx::Text::Model& element, const Execution::Context& ctx, QObject* parent)
    : ProcessComponent_T{element, ctx, "textComponent", parent}
{
  auto n = ossia::make_node<text_node>(
      *ctx.execState, ctx.doc.plugin<DocumentPlugin>().exec);

  for(auto* outlet : element.outlets())
  {
    if(auto out = qobject_cast<Gfx::TextureOutlet*>(outlet))
    {
      out->nodeId = n->id;
    }
  }

  for(std::size_t i = 0; i < 8; i++)
  {
    auto ctrl = qobject_cast<Process::ControlInlet*>(element.inlets()[i]);
    auto& p = n->add_control();
    p->value = ctrl->value();

    QObject::connect(
        ctrl, &Process::ControlInlet::valueChanged, this, con_unvalidated{ctx, i, 0, n});
  }

  n->root_outputs().push_back(new ossia::texture_outlet);

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
