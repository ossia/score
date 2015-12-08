#pragma once
class ConstraintModel;
class EventModel;
class TimeNodeModel;
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_scenario_export.h>
ISCORE_PLUGIN_SCENARIO_EXPORT void updateTimeNodeExtent(
        const Id<TimeNodeModel>& id,
        Scenario::ScenarioModel& s);

// Will call updateTimeNodeExtent
ISCORE_PLUGIN_SCENARIO_EXPORT void updateEventExtent(
        const Id<EventModel>& id,
        Scenario::ScenarioModel& s);

// Will call updateEventExtent
ISCORE_PLUGIN_SCENARIO_EXPORT void updateConstraintVerticalPos(
        double y,
        const Id<ConstraintModel>& id,
        Scenario::ScenarioModel& s);
