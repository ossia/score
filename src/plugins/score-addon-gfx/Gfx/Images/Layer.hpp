#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Gfx/Images/Presenter.hpp>
#include <Gfx/Images/Process.hpp>
#include <Gfx/Images/View.hpp>

namespace Gfx::Images
{
using LayerFactory
    = Process::LayerFactory_T<Gfx::Images::Model, Gfx::Images::Presenter, Gfx::Images::View>;
}
