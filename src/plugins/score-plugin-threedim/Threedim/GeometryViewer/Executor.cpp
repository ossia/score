#include "Executor.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/TexturePort.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/dataflow/port.hpp>

#include <Threedim/GeometryViewer/Node.hpp>
#include <Threedim/GeometryViewer/Process.hpp>

namespace Gfx::GeometryViewer
{
namespace
{
class viewer_node final : public gfx_exec_node
{
public:
  using gfx_exec_node::gfx_exec_node;
  void init() { id = exec_context->ui->register_node(std::make_unique<GeometryViewerNode>()); }
  ~viewer_node() { exec_context->ui->unregister_node(id); }
  std::string label() const noexcept override { return "Gfx::GeometryViewer_node"; }
};
}

ProcessExecutorComponent::ProcessExecutorComponent(
    Gfx::GeometryViewer::Model& element, const Execution::Context& ctx, QObject* parent)
    : ProcessComponent_T{element, ctx, "geometryViewerComponent", parent}
{
  auto n = ossia::make_node<viewer_node>(*ctx.execState, ctx.doc.plugin<DocumentPlugin>().exec);

  // The ports of the node are those of the model, in its order
  element.inlets()[0]->setupExecution(*n->add_texture(), this);
  n->root_inputs().push_back(new ossia::geometry_inlet);
  for(std::size_t i = 2; i < element.inlets().size(); i++)
  {
    auto ctrl = qobject_cast<Process::ControlInlet*>(element.inlets()[i]);
    auto& p = n->add_control();
    ctrl->setupExecution(*n->root_inputs().back(), this);
    p->value = ctrl->value();
    connect(
        ctrl, &Process::ControlInlet::valueChanged, this,
        con_unvalidated{ctx, i - 2, 0, n});
  }
  n->add_texture_out();
  n->init();

  for(auto* outlet : element.outlets())
    if(auto out = qobject_cast<Gfx::TextureOutlet*>(outlet))
      out->nodeId = n->id;

  this->node = n;
  m_ossia_process = std::make_shared<ossia::node_process>(n);
}

void ProcessExecutorComponent::cleanup()
{
  for(auto* outlet : this->process().outlets())
    if(auto out = qobject_cast<TextureOutlet*>(outlet))
      out->nodeId = score::gfx::invalid_node_index;
  ProcessComponent_T::cleanup();
}
}
