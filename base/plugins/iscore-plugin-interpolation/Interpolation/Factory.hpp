#pragma once
#include <Curve/Process/CurveProcessFactory.hpp>
#include <Interpolation/Colors.hpp>
#include <Interpolation/Process.hpp>
#include <Interpolation/Layer.hpp>
#include <Interpolation/Presenter.hpp>
#include <Interpolation/View.hpp>


namespace Interpolation
{
using Factory =
    Curve::CurveProcessFactory_T<
ProcessModel, Layer, Presenter, View, Colors>;
}

