#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_scenario_export.h>
class ConstraintModel;
class StateModel;
class Process;
ISCORE_PLUGIN_SCENARIO_EXPORT void AddProcess(ConstraintModel& constraint, Process*);
ISCORE_PLUGIN_SCENARIO_EXPORT void RemoveProcess(ConstraintModel& constraint, const Id<Process>&);

ISCORE_PLUGIN_SCENARIO_EXPORT void SetPreviousConstraint(StateModel& state, const ConstraintModel& constraint);
ISCORE_PLUGIN_SCENARIO_EXPORT void SetNextConstraint(StateModel& state, const ConstraintModel& constraint);
ISCORE_PLUGIN_SCENARIO_EXPORT void SetNoPreviousConstraint(StateModel& state);
ISCORE_PLUGIN_SCENARIO_EXPORT void SetNoNextConstraint(StateModel& state);
