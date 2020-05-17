#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Nodal/Presenter.hpp>
#include <Nodal/Process.hpp>
#include <Nodal/View.hpp>

namespace Nodal
{
using LayerFactory = Process::LayerFactory_T<Nodal::Model, Nodal::Presenter, Nodal::View>;
}
