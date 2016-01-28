#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_scenario_export.h>

namespace Process { class ProcessModel; }
namespace Scenario
{
class ConstraintModel;
class StateModel;
ISCORE_PLUGIN_SCENARIO_EXPORT void AddProcess(
        ConstraintModel& constraint,
        Process::ProcessModel*);
ISCORE_PLUGIN_SCENARIO_EXPORT void RemoveProcess(
        ConstraintModel& constraint,
        const Id<Process::ProcessModel>&);

ISCORE_PLUGIN_SCENARIO_EXPORT void SetPreviousConstraint(
        StateModel& state,
        const ConstraintModel& constraint);
ISCORE_PLUGIN_SCENARIO_EXPORT void SetNextConstraint(
        StateModel& state,
        const ConstraintModel& constraint);
ISCORE_PLUGIN_SCENARIO_EXPORT void SetNoPreviousConstraint(
        StateModel& state);
ISCORE_PLUGIN_SCENARIO_EXPORT void SetNoNextConstraint(
        StateModel& state);
}
