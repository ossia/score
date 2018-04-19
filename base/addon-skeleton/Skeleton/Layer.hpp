#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/LayerModelPanelProxy.hpp>
#include <Skeleton/Presenter.hpp>
#include <Skeleton/Process.hpp>
#include <Skeleton/View.hpp>

namespace Skeleton
{
using LayerFactory = Process::GenericLayerFactory<
    Skeleton::Model,
    Skeleton::Presenter,
    Skeleton::View,
    Process::GraphicsViewLayerPanelProxy>;
}
