#pragma once
#include <Gfx/Video/Presenter.hpp>
#include <Gfx/Video/Process.hpp>
#include <Gfx/Video/View.hpp>
#include <Process/GenericProcessFactory.hpp>

namespace Gfx::Video
{
using LayerFactory = Process::
    LayerFactory_T<Gfx::Video::Model, Gfx::Video::Presenter, Gfx::Video::View>;
}
