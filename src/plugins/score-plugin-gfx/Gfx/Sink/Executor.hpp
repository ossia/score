#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace Gfx::Sink
{
class Model;
class ProcessExecutorComponent final
    : public Execution::ProcessComponent_T<Gfx::Sink::Model, ossia::node_process>
{
  COMPONENT_METADATA("7e091c01-4c62-49e9-ad2b-bd3c8354ebd0")
public:
  ProcessExecutorComponent(Model& element, const Execution::Context& ctx, QObject* parent);
};

using ProcessExecutorComponentFactory
    = Execution::ProcessComponentFactory_T<ProcessExecutorComponent>;
}
