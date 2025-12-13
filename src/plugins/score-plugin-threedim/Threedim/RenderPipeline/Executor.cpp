#include "Executor.hpp"

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/TexturePort.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>
#include <Threedim/RenderPipeline/RenderPipelineNode.hpp>
#include <Threedim/RenderPipeline/Process.hpp>
#include <ossia/dataflow/port.hpp>
#include <score/document/DocumentContext.hpp>

namespace Gfx::RenderPipeline
{
ProcessExecutorComponent::ProcessExecutorComponent(
    Gfx::RenderPipeline::Model& element, const Execution::Context& ctx, QObject* parent)
    : ISFExecutorComponent{element, ctx, "rpExecutorComponent", parent}
{
  init(element.processedProgram(), ctx);
  connect(
      &element, &RenderPipeline::Model::programChanged, this,
      &ProcessExecutorComponent::on_shaderChanged, Qt::DirectConnection);
}

void ProcessExecutorComponent::on_shaderChanged()
{
  ISFExecutorComponent::on_shaderChanged(
      static_cast<RenderPipeline::Model&>(process()).processedProgram());
}
}
