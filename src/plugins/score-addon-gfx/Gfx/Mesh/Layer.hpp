#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Gfx/Mesh/Presenter.hpp>
#include <Gfx/Mesh/Process.hpp>
#include <Gfx/Mesh/View.hpp>

namespace Gfx::Mesh
{
using LayerFactory
    = Process::LayerFactory_T<Gfx::Mesh::Model, Gfx::Mesh::Presenter, Gfx::Mesh::View>;
}
