#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Media/Step/Model.hpp>
#include <Media/Step/Metadata.hpp>
#include <Media/Step/Presenter.hpp>
#include <Media/Step/View.hpp>
#include <Process/LayerModelPanelProxy.hpp>

namespace Media
{
namespace Step
{
using ProcessFactory = Process::ProcessFactory_T<Step::Model>;
using LayerFactory = Process::LayerFactory_T<
    Step::Model,
    Step::Presenter,
    Step::View,
    Process::GraphicsViewLayerPanelProxy>;
}
}
