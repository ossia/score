#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace Gfx::Filter
{
class Model;
class ProcessExecutorComponent final
    : public Execution::ProcessComponent_T<Gfx::Filter::Model, ossia::node_process>
{
  COMPONENT_METADATA("71a1d1bb-6363-48a7-8495-087a8a0e9436")
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
