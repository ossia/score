#include "Executor.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/dataflow/port.hpp>

#include <3rdparty/icosphere/Icosphere.h>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExec.hpp>
#include <Gfx/Graph/phongnode.hpp>
#include <Gfx/Mesh/Process.hpp>
#include <Gfx/TexturePort.hpp>
namespace Gfx::Mesh
{
class mesh_node final : public gfx_exec_node
{
public:
  mesh_node(const isf::descriptor& isf, const QString& frag, GfxExecutionAction& ctx)
      : gfx_exec_node{ctx}
  {
    static Icosphere ico{0.5, 3, false}; //, 1, false};
    static auto mesh = gsl::span<const float>(
        ico.getInterleavedVertices(), ico.getInterleavedVertexSize() / sizeof(float));
    static auto idx = gsl::span<const unsigned int>(ico.getIndices(), ico.getIndexCount());
    static TextureNormalMesh icosahedron{mesh, idx, (int)ico.getVertexCount()};
    auto n = std::make_unique<PhongNode>(&icosahedron);

    id = exec_context->ui->register_node(std::move(n));
  }

  ~mesh_node() { exec_context->ui->unregister_node(id); }

  std::string label() const noexcept override { return "Gfx::filter_node"; }
};

ProcessExecutorComponent::ProcessExecutorComponent(
    Gfx::Mesh::Model& element,
    const Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : ProcessComponent_T{element, ctx, id, "gfxExecutorComponent", parent}
{
  try
  {

    const auto& desc = element.isfDescriptor();

    auto n = std::make_shared<mesh_node>(
        desc, element.processedFragment(), ctx.doc.plugin<DocumentPlugin>().exec);

    n->root_outputs().push_back(new ossia::texture_outlet);

    this->node = n;
    m_ossia_process = std::make_shared<ossia::node_process>(n);
  }
  catch (...)
  {
  }
}
}
