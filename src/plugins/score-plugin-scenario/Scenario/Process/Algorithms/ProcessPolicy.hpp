#pragma once
#include <score/model/Identifier.hpp>

#include <score_plugin_scenario_export.h>

namespace Process
{
class ProcessModel;
}
namespace Scenario
{
class IntervalModel;
class StateModel;
SCORE_PLUGIN_SCENARIO_EXPORT void
AddProcess(IntervalModel& interval, Process::ProcessModel*);

// Register / unregister a process's start/end state data on a state
// (e.g. when an interval is rewired to end on a different state).
SCORE_PLUGIN_SCENARIO_EXPORT void
AddProcessBeforeState(StateModel& statemodel, const Process::ProcessModel& proc);
SCORE_PLUGIN_SCENARIO_EXPORT void
AddProcessAfterState(StateModel& statemodel, const Process::ProcessModel& proc);
SCORE_PLUGIN_SCENARIO_EXPORT void
RemoveProcessBeforeState(StateModel& statemodel, const Process::ProcessModel& proc);
SCORE_PLUGIN_SCENARIO_EXPORT void
RemoveProcessAfterState(StateModel& statemodel, const Process::ProcessModel& proc);

// Does delete the process
SCORE_PLUGIN_SCENARIO_EXPORT void
RemoveProcess(IntervalModel& interval, const Id<Process::ProcessModel>&);

// Does not
SCORE_PLUGIN_SCENARIO_EXPORT void
EraseProcess(IntervalModel& interval, const Id<Process::ProcessModel>&);

SCORE_PLUGIN_SCENARIO_EXPORT void
SetPreviousInterval(StateModel& state, const IntervalModel& interval);
SCORE_PLUGIN_SCENARIO_EXPORT void
SetNextInterval(StateModel& state, const IntervalModel& interval);
SCORE_PLUGIN_SCENARIO_EXPORT void SetNoPreviousInterval(StateModel& state);
SCORE_PLUGIN_SCENARIO_EXPORT void SetNoNextInterval(StateModel& state);
}
