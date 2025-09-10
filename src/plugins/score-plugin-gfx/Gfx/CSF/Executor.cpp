#include "Executor.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>

#include <Gfx/CSF/Process.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/ISFExecutorNode.hpp>
#include <Gfx/TexturePort.hpp>
#include <Gfx/VSA/Process.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/dataflow/graph_edge_helpers.hpp>
#include <ossia/dataflow/port.hpp>
namespace Gfx::CSF
{
ProcessExecutorComponent::ProcessExecutorComponent(
    Gfx::CSF::Model& element, const Execution::Context& ctx, QObject* parent)
    : ISFExecutorComponent{element, ctx, "isfExecutorComponent", parent}
{
  init(element.processedCompute(), element.descriptor(), ctx);
  connect(
      &element, &CSF::Model::programChanged, this,
      &ProcessExecutorComponent::on_shaderChanged, Qt::DirectConnection);
}

void ProcessExecutorComponent::on_shaderChanged()
{
  auto& element = static_cast<CSF::Model&>(process());
  ISFExecutorComponent::on_shaderChanged(
      element.processedCompute(), element.descriptor());
}
}
