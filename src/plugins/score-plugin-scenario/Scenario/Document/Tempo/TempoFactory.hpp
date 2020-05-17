#pragma once
#include <Automation/AutomationColors.hpp>
#include <Curve/Process/CurveProcessFactory.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/HeaderDelegate.hpp>
#include <Scenario/Document/Tempo/TempoProcess.hpp>
#include <Scenario/Document/Tempo/TempoView.hpp>
namespace Scenario
{

using TempoFactory = Process::ProcessFactory_T<Scenario::TempoProcess>;
using TempoLayerFactory = Curve::CurveLayerFactory_T<
    Scenario::TempoProcess,
    Scenario::TempoPresenter,
    Scenario::TempoView,
    Automation::Colors,
    Process::DefaultHeaderDelegate>;

}
