#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/WidgetLayer/WidgetProcessFactory.hpp>
#include <Media/Effect/EffectProcessModel.hpp>
#include <Media/Effect/EffectProcessMetadata.hpp>
#include <Media/Effect/Effect/Widgets/EffectListWidget.hpp>
namespace Media
{
namespace Effect
{
using ProcessFactory = Process::GenericProcessModelFactory<Effect::ProcessModel>;
using LayerFactory = WidgetLayer::LayerFactory<Effect::ProcessModel, EffectListWidget>;
}
}
