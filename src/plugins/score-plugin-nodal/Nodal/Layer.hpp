#pragma once
#include <Nodal/Presenter.hpp>
#include <Nodal/Process.hpp>
#include <Nodal/View.hpp>
#include <Process/GenericProcessFactory.hpp>

namespace Nodal
{
using LayerFactory
    = Process::LayerFactory_T<Nodal::Model, Nodal::Presenter, Nodal::View>;
}
