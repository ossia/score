#include "Executor.hpp"

#include <Gfx/ISFExecutorNode.hpp>
#include <Gfx/VSA/Process.hpp>
namespace Gfx::VSA
{
ProcessExecutorComponent::ProcessExecutorComponent(
    Gfx::VSA::Model& element, const Execution::Context& ctx, QObject* parent)
    : ISFExecutorComponent{element, ctx, "isfExecutorComponent", parent}
{
  init(element.processedProgram(), ctx);
  connect(
      &element, &VSA::Model::programChanged, this,
      &ProcessExecutorComponent::on_shaderChanged, Qt::DirectConnection);
}

void ProcessExecutorComponent::on_shaderChanged()
{
  ISFExecutorComponent::on_shaderChanged(
      static_cast<VSA::Model&>(process()).processedProgram());
}
}
