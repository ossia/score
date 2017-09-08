#pragma once
#include <iscore/model/Identifier.hpp>
#include <iscore_plugin_scenario_export.h>

namespace Process
{
class ProcessModel;
}
namespace Scenario
{
class IntervalModel;
class StateModel;
ISCORE_PLUGIN_SCENARIO_EXPORT void
AddProcess(IntervalModel& interval, Process::ProcessModel*);
ISCORE_PLUGIN_SCENARIO_EXPORT void
RemoveProcess(IntervalModel& interval, const Id<Process::ProcessModel>&);

ISCORE_PLUGIN_SCENARIO_EXPORT void
SetPreviousInterval(StateModel& state, const IntervalModel& interval);
ISCORE_PLUGIN_SCENARIO_EXPORT void
SetNextInterval(StateModel& state, const IntervalModel& interval);
ISCORE_PLUGIN_SCENARIO_EXPORT void SetNoPreviousInterval(StateModel& state);
ISCORE_PLUGIN_SCENARIO_EXPORT void SetNoNextInterval(StateModel& state);
}
