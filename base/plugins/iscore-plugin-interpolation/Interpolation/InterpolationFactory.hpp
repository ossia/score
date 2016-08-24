#pragma once
#include <Curve/Process/CurveProcessFactory.hpp>
#include <Interpolation/InterpolationColors.hpp>
#include <Interpolation/InterpolationProcess.hpp>
#include <Interpolation/InterpolationLayer.hpp>
#include <Interpolation/InterpolationPresenter.hpp>
#include <Interpolation/InterpolationView.hpp>


namespace Interpolation
{
using Factory =
    Curve::CurveProcessFactory_T<
ProcessModel, Layer, Presenter, View, Colors>;
}

