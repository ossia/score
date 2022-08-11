#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace Gfx::Text
{
class Model;
class ProcessExecutorComponent final
    : public Execution::ProcessComponent_T<Gfx::Text::Model, ossia::node_process>
{
  COMPONENT_METADATA("1af00601-84ad-49d7-a854-f1ea79c5c8a9")
public:
  ProcessExecutorComponent(
      Model& element, const Execution::Context& ctx, QObject* parent);
  void cleanup() override;
};

using ProcessExecutorComponentFactory
    = Execution::ProcessComponentFactory_T<ProcessExecutorComponent>;
}
