#pragma once
#include <Curve/Process/CurveProcessFactory.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/HeaderDelegate.hpp>

#include <Interpolation/InterpolationColors.hpp>
#include <Interpolation/InterpolationPresenter.hpp>
#include <Interpolation/InterpolationProcess.hpp>
#include <Interpolation/InterpolationView.hpp>

namespace Interpolation
{
using InterpolationFactory = Process::ProcessFactory_T<ProcessModel>;
using InterpolationLayerFactory = Curve::
    CurveLayerFactory_T<ProcessModel, Presenter, View, Colors, Process::DefaultHeaderDelegate>;
}
