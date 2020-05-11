#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Gfx/Video/Presenter.hpp>
#include <Gfx/Video/Process.hpp>
#include <Gfx/Video/View.hpp>

namespace Gfx::Video
{
using LayerFactory = Process::
    LayerFactory_T<Gfx::Video::Model, Gfx::Video::Presenter, Gfx::Video::View>;
}
