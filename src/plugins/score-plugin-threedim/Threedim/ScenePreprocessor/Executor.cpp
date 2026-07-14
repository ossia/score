#include "Executor.hpp"

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>
#include <Threedim/ScenePreprocessor/Process.hpp>

#include <ossia/dataflow/port.hpp>

#include <score/document/DocumentContext.hpp>

namespace Gfx::ScenePreprocessor
{
class scene_preprocessor_exec_node final : public gfx_exec_node
{
public:
  scene_preprocessor_exec_node(GfxExecutionAction& ctx)
      : gfx_exec_node{ctx}
  {
  }

  void init()
  {
    auto node = std::make_unique<score::gfx::ScenePreprocessorNode>();
    id = exec_context->ui->register_node(std::move(node));
  }

  ~scene_preprocessor_exec_node() { exec_context->ui->unregister_node(id); }

  std::string label() const noexcept override { return "Gfx::ScenePreprocessor_node"; }
};

ProcessExecutorComponent::ProcessExecutorComponent(
    Gfx::ScenePreprocessor::Model& element,
    const Execution::Context& ctx,
    QObject* parent)
    : ProcessComponent_T{element, ctx, "scenePreprocessorComponent", parent}
{
  auto n = ossia::make_node<scene_preprocessor_exec_node>(
      *ctx.execState, ctx.doc.plugin<DocumentPlugin>().exec);

  // Port 0: Scene input
  n->add_geometry();
  // Single Geometry outlet — material-texture arrays (base_color,
  // metal_rough, normal, emissive) and the skybox ride along as
  // auxiliary_texture entries on the emitted geometry; scene-wide
  // UBOs/SSBOs (camera, env, scene_lights/materials, per_draws,
  // indirect, scene_counts) ride along as auxiliary_buffer entries.
  // Consumer shaders bind everything by name.
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
