#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <Gfx/ISFExecutor.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace Gfx::CSF
{
class Model;
class ProcessExecutorComponent final : public ISFExecutorComponent
{
  COMPONENT_METADATA("a0fbfd0a-c3dd-4aab-9a5b-0c5e99dfe334")
public:
  using model_type = Gfx::CSF::Model;
  ProcessExecutorComponent(
      Model& element, const Execution::Context& ctx, QObject* parent);
  void on_shaderChanged();
};

using ProcessExecutorComponentFactory
    = Execution::ProcessComponentFactory_T<ProcessExecutorComponent>;
}
