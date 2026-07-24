#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace Gfx::ScenePreprocessor
{
class Model;
class ProcessExecutorComponent final
    : public Execution::
          ProcessComponent_T<Gfx::ScenePreprocessor::Model, ossia::node_process>
{
  COMPONENT_METADATA("d7e2f8b4-9a3c-4e1b-8f6d-0c5a2b7e9f1d")
public:
  ProcessExecutorComponent(
      Model& element,
      const Execution::Context& ctx,
      QObject* parent);
  void cleanup() override;
};

using ProcessExecutorComponentFactory
    = Execution::ProcessComponentFactory_T<ProcessExecutorComponent>;
}
