#include "Executor.hpp"

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExec.hpp>
#include <Gfx/Graph/TextNode.hpp>
#include <Gfx/Text/Process.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/dataflow/port.hpp>

namespace Gfx::Text
{
class text_node final : public gfx_exec_node
{
public:
  text_node(GfxExecutionAction& ctx)
      : gfx_exec_node{ctx}
  {
    id = exec_context->ui->register_node(std::make_unique<score::gfx::TextNode>());
  }

  ~text_node()
  {
    if (id >= 0)
      exec_context->ui->unregister_node(id);
  }

  std::string label() const noexcept override { return "Gfx::text_node"; }
};

ProcessExecutorComponent::ProcessExecutorComponent(
    Gfx::Text::Model& element,
    const Execution::Context& ctx,
    QObject* parent)
    : ProcessComponent_T{element, ctx, "textComponent", parent}
{
  auto n = std::make_shared<text_node>(ctx.doc.plugin<DocumentPlugin>().exec);

  for (std::size_t i = 0; i < 8; i++)
  {
    auto ctrl = qobject_cast<Process::ControlInlet*>(element.inlets()[i]);
    auto& p = n->add_control();
    p->value = ctrl->value();
    p->changed = true;

    QObject::connect(
        ctrl,
        &Process::ControlInlet::valueChanged,
        this,
        con_unvalidated{ctx, i, n});
  }

  n->root_outputs().push_back(new ossia::texture_outlet);

  this->node = n;
  m_ossia_process = std::make_shared<ossia::node_process>(n);
}
}
