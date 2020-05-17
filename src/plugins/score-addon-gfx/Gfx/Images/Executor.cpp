#include "Executor.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/dataflow/port.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExec.hpp>
#include <Gfx/Graph/imagenode.hpp>
#include <Gfx/Images/Process.hpp>

namespace Gfx::Images
{
class image_node final : public gfx_exec_node
{
public:
  image_node(const std::vector<Image>& dec, GfxExecutionAction& ctx) : gfx_exec_node{ctx}
  {
    id = exec_context->ui->register_node(std::make_unique<ImagesNode>(dec));
  }

  ~image_node()
  {
    if (id >= 0)
      exec_context->ui->unregister_node(id);
  }

  std::string label() const noexcept override { return "Gfx::image_node"; }
};

ProcessExecutorComponent::ProcessExecutorComponent(
    Gfx::Images::Model& element,
    const Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : ProcessComponent_T{element, ctx, id, "gfxExecutorComponent", parent}
{
  auto n = std::make_shared<image_node>(element.images(), ctx.doc.plugin<DocumentPlugin>().exec);

  for (int i = 0; i < 2; i++)
  {
    auto ctrl = qobject_cast<Process::ControlInlet*>(element.inlets()[i]);
    auto& p = n->add_control();
    *p.value = ctrl->value(); // TODO does this make sense ?
    p.changed = true;         // we will send the first value

    QObject::connect(ctrl, &Process::ControlInlet::valueChanged, this, con_unvalidated{ctx, i, n});
  }

  n->root_outputs().push_back(new ossia::value_outlet);

  this->node = n;
  m_ossia_process = std::make_shared<ossia::node_process>(n);
}
}
