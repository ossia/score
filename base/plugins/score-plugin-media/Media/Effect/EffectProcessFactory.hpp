#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Media/Effect/EffectProcessLayer.hpp>
namespace Media
{
namespace Effect
{
using ProcessFactory = Process::ProcessFactory_T<Effect::ProcessModel>;

using LayerFactory = Process::
    LayerFactory_T<Effect::ProcessModel, Effect::Presenter, Effect::View, Process::GraphicsViewLayerPanelProxy>;
}
}
