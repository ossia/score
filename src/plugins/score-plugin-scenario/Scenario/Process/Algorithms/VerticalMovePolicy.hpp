#pragma once

#include <score/model/Identifier.hpp>

#include <score_plugin_scenario_export.h>

namespace Scenario
{
class IntervalModel;
class EventModel;
class ProcessModel;
class TimeSyncModel;
SCORE_PLUGIN_SCENARIO_EXPORT void
updateTimeSyncExtent(const Id<TimeSyncModel>& id, const Scenario::ProcessModel& s);

// Will call updateTimeSyncExtent
SCORE_PLUGIN_SCENARIO_EXPORT void
updateEventExtent(const Id<EventModel>& id, const Scenario::ProcessModel& s);

// Will call updateEventExtent
SCORE_PLUGIN_SCENARIO_EXPORT void updateIntervalVerticalPos(
    double y,
    const Id<IntervalModel>& id,
    const Scenario::ProcessModel& s);
}
