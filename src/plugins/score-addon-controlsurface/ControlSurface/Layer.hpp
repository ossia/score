#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <ControlSurface/Presenter.hpp>
#include <ControlSurface/Process.hpp>
#include <ControlSurface/View.hpp>

namespace ControlSurface
{
using LayerFactory = Process::
    LayerFactory_T<ControlSurface::Model, ControlSurface::Presenter, ControlSurface::View>;
}
