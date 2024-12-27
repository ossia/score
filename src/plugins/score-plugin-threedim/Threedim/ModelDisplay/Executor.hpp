#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace Gfx::ModelDisplay
{
class Model;
class ProcessExecutorComponent final
    : public Execution::
          ProcessComponent_T<Gfx::ModelDisplay::Model, ossia::node_process>
{
  COMPONENT_METADATA("f72d3fcd-a4d2-42cb-8758-94c0d72ffe61")
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
