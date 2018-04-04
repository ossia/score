#pragma once
#include <Media/Effect/VST/VSTEffectModel.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <ossia/dataflow/node_process.hpp>

namespace Engine::Execution
{
class VSTEffectComponent final
    : public Engine::Execution::ProcessComponent_T<Media::VST::VSTEffectModel, ossia::node_process>
{
    Q_OBJECT
    COMPONENT_METADATA("84bb8af9-bfb9-4819-8427-79787de716f3")

    public:
      static constexpr bool is_unique = true;

    VSTEffectComponent(
        Media::VST::VSTEffectModel& proc,
        const Engine::Execution::Context& ctx,
        const Id<score::Component>& id,
        QObject* parent);
};
using VSTEffectComponentFactory = Engine::Execution::ProcessComponentFactory_T<VSTEffectComponent>;
}

