#pragma once
#include <Media/Step/Model.hpp>
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace Execution
{

class StepComponent final
    : public ::Execution::ProcessComponent_T<Media::Step::Model, ossia::node_process>
{
  COMPONENT_METADATA("5b9c03cb-d062-40ee-b2a2-88279b088d4d")
public:
  StepComponent(
      Media::Step::Model& element,
      const ::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  void recompute();

  ~StepComponent();

private:
};

using StepComponentFactory = ::Execution::ProcessComponentFactory_T<StepComponent>;
}
