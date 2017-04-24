#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Loop/LoopLayer.hpp>
#include <Loop/LoopPresenter.hpp>
#include <Loop/LoopProcessMetadata.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <Loop/LoopView.hpp>
#include <Process/LayerModelPanelProxy.hpp>

namespace Loop
{
using ProcessFactory = Process::GenericProcessModelFactory<Loop::ProcessModel>;
using LayerFactory = Process::
    GenericLayerFactory<Loop::ProcessModel, Loop::LayerPresenter, Loop::LayerView, Process::GraphicsViewLayerPanelProxy>;
}
