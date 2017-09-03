#pragma once
#include <Skeleton/Process.hpp>
#include <Skeleton/Presenter.hpp>
#include <Skeleton/View.hpp>
#include <Process/LayerModelPanelProxy.hpp>
#include <Process/GenericProcessFactory.hpp>

namespace Skeleton
{
    using LayerFactory = Process::GenericLayerFactory<
    Skeleton::Model,
    Skeleton::Presenter,
    Skeleton::View,
    Process::GraphicsViewLayerPanelProxy>;
}
