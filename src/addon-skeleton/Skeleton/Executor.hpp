#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace Skeleton
{
class Model;
class ProcessExecutorComponent final
    : public Execution::ProcessComponent_T<
          Skeleton::Model, ossia::node_process>
{
  COMPONENT_METADATA("00000000-0000-0000-0000-000000000000")
public:
  ProcessExecutorComponent(
      Model& element, const Execution::Context& ctx, QObject* parent);
};

using ProcessExecutorComponentFactory
    = Execution::ProcessComponentFactory_T<ProcessExecutorComponent>;
}
