#pragma once
#include <Curve/Process/CurveProcessFactory.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/HeaderDelegate.hpp>

#include <InterpState/InterpStateColors.hpp>
#include <InterpState/InterpStatePresenter.hpp>
#include <InterpState/InterpStateProcess.hpp>
#include <InterpState/InterpStateView.hpp>

namespace InterpState
{
using InterpStateFactory = Process::ProcessFactory_T<ProcessModel>;
using InterpStateLayerFactory = Curve::
    CurveLayerFactory_T<ProcessModel, Presenter, View, Colors, Process::DefaultHeaderDelegate>;
}
