#pragma once
#include <ControlSurface/Presenter.hpp>
#include <ControlSurface/Process.hpp>
#include <ControlSurface/View.hpp>
#include <Process/GenericProcessFactory.hpp>

namespace ControlSurface
{
using LayerFactory = Process::LayerFactory_T<
    ControlSurface::Model,
    ControlSurface::Presenter,
    ControlSurface::View>;
}
