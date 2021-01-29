#pragma once
#include <Vst/EffectModel.hpp>
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

#include <verdigris>

namespace Vst
{
class Executor final
    : public Execution::ProcessComponent_T<Vst::Model, ossia::node_process>
{
  W_OBJECT(Executor)
  COMPONENT_METADATA("84bb8af9-bfb9-4819-8427-79787de716f3")

public:
  static constexpr bool is_unique = true;

  Executor(
      Vst::Model& proc,
      const Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

private:
  template <typename Node_T>
  void setupNode(Node_T& node);
};
using ExecutorFactory = Execution::ProcessComponentFactory_T<Executor>;
}
