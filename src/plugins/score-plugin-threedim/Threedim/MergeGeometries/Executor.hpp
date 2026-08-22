#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace Gfx::MergeGeometries
{
class Model;
class ProcessExecutorComponent final
    : public Execution::
          ProcessComponent_T<Gfx::MergeGeometries::Model, ossia::node_process>
{
  COMPONENT_METADATA("b7c8d9e0-f1a2-4b3c-8d4e-5f6a7b8c9d0e")
public:
  ProcessExecutorComponent(
      Model& element, const Execution::Context& ctx, QObject* parent);
  void cleanup() override;
};

using ProcessExecutorComponentFactory
    = Execution::ProcessComponentFactory_T<ProcessExecutorComponent>;
}
