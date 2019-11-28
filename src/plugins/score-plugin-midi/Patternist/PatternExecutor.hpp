#pragma once
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionContext.hpp>

#include <ossia/dataflow/node_process.hpp>
#include <ossia/editor/scenario/time_process.hpp>

namespace Patternist
{
class ProcessModel;
class Executor final
    : public ::Execution::
          ProcessComponent_T<Patternist::ProcessModel, ossia::node_process>,
      public Nano::Observer
{
  COMPONENT_METADATA("6d5334a5-7b8c-45df-9805-11d1b4472cdf")
public:
  static const constexpr bool is_unique = true;
  Executor(
      Patternist::ProcessModel& element,
      const Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);
  ~Executor() override;

private:
};

using ExecutorFactory = ::Execution::ProcessComponentFactory_T<Executor>;
}
