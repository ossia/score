#pragma once
#include <Vst3/EffectModel.hpp>
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

#include <verdigris>

namespace vst3
{
class Executor final
    : public Execution::ProcessComponent_T<vst3::Model, ossia::node_process>
{
  W_OBJECT(Executor)
  COMPONENT_METADATA("a9f2c738-d22b-4654-9f58-25f25f099d79")

public:
  static constexpr bool is_unique = true;

  Executor(
      vst3::Model& proc,
      const Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

private:
  template <typename Node_T>
  void setupNode(Node_T& node);
};
using ExecutorFactory = Execution::ProcessComponentFactory_T<Executor>;
}
