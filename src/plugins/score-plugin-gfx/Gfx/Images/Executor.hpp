#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace Gfx::Images
{
class Model;
class ProcessExecutorComponent final
    : public Execution::
          ProcessComponent_T<Gfx::Images::Model, ossia::node_process>
{
  COMPONENT_METADATA("81e652e2-e369-44d0-9c36-979a369ac465")
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
