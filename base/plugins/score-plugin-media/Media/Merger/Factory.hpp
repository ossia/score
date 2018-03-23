#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Media/Merger/Model.hpp>
#include <Media/Merger/Metadata.hpp>
#include <Media/Merger/Presenter.hpp>
#include <Media/Merger/View.hpp>
#include <Process/LayerModelPanelProxy.hpp>
#include <Effect/EffectFactory.hpp>
#include <Media/Effect/DefaultEffectItem.hpp>

namespace Media
{
namespace Merger
{
using ProcessFactory = Process::ProcessFactory_T<Merger::Model>;
using LayerFactory = Process::EffectLayerFactory_T<
    Merger::Model, Media::Effect::DefaultEffectItem>;
}
}
