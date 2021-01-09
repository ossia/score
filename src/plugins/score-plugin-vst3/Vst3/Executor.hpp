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
  COMPONENT_METADATA("1693023f-9f28-498c-8d43-c6f9c741a476")

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
using VSTEffectComponentFactory = Execution::ProcessComponentFactory_T<Executor>;
}
