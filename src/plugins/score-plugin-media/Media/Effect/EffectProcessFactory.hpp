#pragma once
#include <Media/Effect/EffectProcessLayer.hpp>
#include <Process/GenericProcessFactory.hpp>
namespace Media
{
namespace Effect
{
using ProcessFactory = Process::ProcessFactory_T<Effect::ProcessModel>;

using LayerFactory = Process::LayerFactory_T<
    Effect::ProcessModel, Effect::Presenter, Effect::View>;
}
}
