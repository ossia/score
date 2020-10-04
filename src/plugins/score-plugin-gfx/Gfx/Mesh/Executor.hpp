#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace Gfx::Mesh
{
class Model;
class ProcessExecutorComponent final
    : public Execution::ProcessComponent_T<Gfx::Mesh::Model, ossia::node_process>
{
  COMPONENT_METADATA("a8f9d29f-4fb3-4874-a549-e94ecf0e3bd6")
public:
  ProcessExecutorComponent(
      Model& element,
      const Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);
};

using ProcessExecutorComponentFactory
    = Execution::ProcessComponentFactory_T<ProcessExecutorComponent>;
}
