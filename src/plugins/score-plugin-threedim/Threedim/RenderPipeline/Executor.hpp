#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <Gfx/ISFExecutor.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace Gfx::RenderPipeline
{
class Model;
class ProcessExecutorComponent final : public ISFExecutorComponent
{
  COMPONENT_METADATA("84d34449-0015-476d-b3a8-e9f5c8241389")
public:
  using model_type = Gfx::RenderPipeline::Model;
  ProcessExecutorComponent(
      Model& element, const Execution::Context& ctx, QObject* parent);
  void on_shaderChanged();
};

using ProcessExecutorComponentFactory
    = Execution::ProcessComponentFactory_T<ProcessExecutorComponent>;
}
