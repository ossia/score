#pragma once
#include <Media/Effect/VST/VSTEffectModel.hpp>
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

#include <wobjectdefs.h>

namespace Execution
{
class VSTEffectComponent final
    : public Execution::
          ProcessComponent_T<Media::VST::VSTEffectModel, ossia::node_process>
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
};
using VSTEffectComponentFactory
    = Execution::ProcessComponentFactory_T<VSTEffectComponent>;
}
