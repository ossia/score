#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Gfx/Filter/Presenter.hpp>
#include <Gfx/Filter/Process.hpp>
#include <Gfx/Filter/View.hpp>

namespace Gfx::Filter
{
using LayerFactory = Process::LayerFactory_T<
    Gfx::Filter::Model,
    Gfx::Filter::Presenter,
    Gfx::Filter::View>;
}
