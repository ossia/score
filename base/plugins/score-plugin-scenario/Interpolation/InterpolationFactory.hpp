#pragma once
#include <Curve/Process/CurveProcessFactory.hpp>
#include <Interpolation/InterpolationColors.hpp>
#include <Interpolation/InterpolationPresenter.hpp>
#include <Interpolation/InterpolationProcess.hpp>
#include <Interpolation/InterpolationView.hpp>
#include <Process/GenericProcessFactory.hpp>

namespace Interpolation
{
using InterpolationFactory = Process::ProcessFactory_T<ProcessModel>;
using InterpolationLayerFactory
    = Curve::CurveLayerFactory_T<ProcessModel, Presenter, View, Colors>;
}
