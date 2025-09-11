#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace Gfx::BufferGeometry
{
class Model;
class ProcessExecutorComponent final
    : public Execution::ProcessComponent_T<Gfx::BufferGeometry::Model, ossia::node_process>
{
  COMPONENT_METADATA("f1e2d3c4-b5a6-9788-1234-567890abcdef")
public:
  ProcessExecutorComponent(
      Model& element, const Execution::Context& ctx, QObject* parent);

  void cleanup() override;
  ~ProcessExecutorComponent();
};

using ProcessExecutorComponentFactory
    = Execution::ProcessComponentFactory_T<ProcessExecutorComponent>;
}