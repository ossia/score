#include "Executor.hpp"

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/Graph/ImageNode.hpp>
#include <Gfx/Images/ImageListChooser.hpp>
#include <Gfx/Images/Process.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/dataflow/port.hpp>

namespace Gfx::Images
{
class image_node final : public gfx_exec_node
{
public:
  image_node(GfxExecutionAction& ctx)
      : gfx_exec_node{ctx}
  {
    id = exec_context->ui->register_node(std::make_unique<score::gfx::ImagesNode>());
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
    QObject* parent)
    : ProcessComponent_T{element, ctx, "gfxExecutorComponent", parent}
{
  auto n = ossia::make_node<image_node>(*ctx.execState,ctx.doc.plugin<DocumentPlugin>().exec);

  // Normal controls
  for (std::size_t i = 0; i < 6; i++)
  {
    auto ctrl = qobject_cast<Process::ControlInlet*>(element.inlets()[i]);
    auto& p = n->add_control();
    p->value = ctrl->value();

    p->changed = true;

    QObject::connect(
        ctrl,
        &Process::ControlInlet::valueChanged,
        this,
        con_unvalidated{ctx, i, 0, n});
  }

  n->root_outputs().push_back(new ossia::texture_outlet);

  this->node = n;
  m_ossia_process = std::make_shared<ossia::node_process>(n);
}

ProcessExecutorComponent::~ProcessExecutorComponent()
{
}

}
