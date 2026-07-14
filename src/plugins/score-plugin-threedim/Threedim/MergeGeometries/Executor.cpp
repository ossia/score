#include "Executor.hpp"

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/Graph/MergeGeometriesNode.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>
#include <Threedim/MergeGeometries/Process.hpp>

#include <ossia/dataflow/port.hpp>

#include <score/document/DocumentContext.hpp>

namespace Gfx::MergeGeometries
{
class merge_geometries_exec_node final : public gfx_exec_node
{
public:
  merge_geometries_exec_node(GfxExecutionAction& ctx)
      : gfx_exec_node{ctx}
  {
  }

  void init()
  {
    auto node = std::make_unique<score::gfx::MergeGeometriesNode>();
    id = exec_context->ui->register_node(std::move(node));
  }

  ~merge_geometries_exec_node() { exec_context->ui->unregister_node(id); }

  std::string label() const noexcept override { return "Gfx::MergeGeometries_node"; }
};

ProcessExecutorComponent::ProcessExecutorComponent(
    Gfx::MergeGeometries::Model& element,
    const Execution::Context& ctx,
    QObject* parent)
    : ProcessComponent_T{element, ctx, "mergeGeometriesComponent", parent}
{
  auto n = ossia::make_node<merge_geometries_exec_node>(
      *ctx.execState, ctx.doc.plugin<DocumentPlugin>().exec);

  for(int i = 0; i < 8; ++i)
    n->add_geometry();
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
