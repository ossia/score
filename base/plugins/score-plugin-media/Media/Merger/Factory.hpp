#pragma once
#include <Effect/EffectFactory.hpp>
#include <Media/Effect/DefaultEffectItem.hpp>
#include <Media/Merger/Metadata.hpp>
#include <Media/Merger/Model.hpp>
#include <Media/Merger/Presenter.hpp>
#include <Media/Merger/View.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/LayerModelPanelProxy.hpp>

namespace Media
{
namespace Merger
{
using ProcessFactory = Process::ProcessFactory_T<Merger::Model>;
using LayerFactory = Process::
    EffectLayerFactory_T<Merger::Model, Media::Effect::DefaultEffectItem>;
}
}
