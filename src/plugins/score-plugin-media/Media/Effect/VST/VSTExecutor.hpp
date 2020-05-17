#pragma once
#if defined(HAS_VST2)
#include <Media/Effect/VST/VSTEffectModel.hpp>
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

#include <verdigris>

namespace Execution
{
class VSTEffectComponent final
    : public Execution::ProcessComponent_T<Media::VST::VSTEffectModel, ossia::node_process>
{
  W_OBJECT(VSTEffectComponent)
  COMPONENT_METADATA("84bb8af9-bfb9-4819-8427-79787de716f3")

public:
  static constexpr bool is_unique = true;

  VSTEffectComponent(
      Media::VST::VSTEffectModel& proc,
      const Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

private:
  template <typename Node_T>
  void setupNode(Node_T& node);
};
using VSTEffectComponentFactory = Execution::ProcessComponentFactory_T<VSTEffectComponent>;
}
#endif
