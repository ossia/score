#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Skeleton/Presenter.hpp>
#include <Skeleton/Process.hpp>
#include <Skeleton/View.hpp>

namespace Skeleton
{
using LayerFactory = Process::LayerFactory_T<
    Skeleton::Model, Skeleton::Presenter, Skeleton::View>;
}
