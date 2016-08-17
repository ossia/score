#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Loop/LoopProcessModel.hpp>
#include <Loop/LoopLayer.hpp>
#include <Loop/LoopPresenter.hpp>
#include <Loop/LoopView.hpp>
#include <Loop/LoopProcessMetadata.hpp>
#include <Process/LayerModelPanelProxy.hpp>

namespace Loop
{
using ProcessFactory = Process::GenericProcessFactory<
    Loop::ProcessModel,
    Loop::Layer,
    Loop::LayerPresenter,
    Loop::LayerView,
    Process::GraphicsViewLayerModelPanelProxy>;
}
