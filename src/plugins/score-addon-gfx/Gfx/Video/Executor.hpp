#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace Gfx::Video
{
class Model;
class ProcessExecutorComponent final
    : public Execution::
          ProcessComponent_T<Gfx::Video::Model, ossia::node_process>
{
  COMPONENT_METADATA("d3d610aa-aa6c-40e6-976b-8844cf48c8ce")
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
