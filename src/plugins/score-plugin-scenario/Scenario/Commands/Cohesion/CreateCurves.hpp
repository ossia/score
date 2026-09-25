#pragma once
#include <Device/Address/AddressSettings.hpp>

#include <score_plugin_scenario_export.h>

#include <QList>

#include <vector>
namespace Process
{
class ProcessModel;
}
namespace score
{
class CommandStackFacade;
struct DocumentContext;
}
namespace Scenario
{
namespace Command
{
class Macro;
}
class IntervalModel;

std::vector<Process::ProcessModel*> CreateCurvesFromAddress(
    const Scenario::IntervalModel& interval,
    const Device::FullAddressAccessorSettings& as, Scenario::Command::Macro& m);

std::vector<Process::ProcessModel*> CreateCurvesFromAddresses(
    const Scenario::IntervalModel& interval,
    const std::vector<Device::FullAddressSettings>& a, Scenario::Command::Macro& m);

SCORE_PLUGIN_SCENARIO_EXPORT void CreateCurves(
    const std::vector<const Scenario::IntervalModel*>& selected_intervals,
    const score::CommandStackFacade& stack);
void CreateCurvesFromAddresses(
    const std::vector<const Scenario::IntervalModel*>& selected_intervals,
    const std::vector<Device::FullAddressSettings>& addresses,
    const score::CommandStackFacade& stack);
}
