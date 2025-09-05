#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <Gfx/ISFExecutor.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace Gfx::VSA
{
class Model;
class ProcessExecutorComponent final : public ISFExecutorComponent
{
  COMPONENT_METADATA("e54dbad3-5a73-4ad1-a3b9-aaa85c5ab5b0")
public:
  using model_type = Gfx::VSA::Model;
  ProcessExecutorComponent(
      Model& element, const Execution::Context& ctx, QObject* parent);
  void on_shaderChanged();
};

using ProcessExecutorComponentFactory
    = Execution::ProcessComponentFactory_T<ProcessExecutorComponent>;
}
