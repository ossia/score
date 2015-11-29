#pragma once
class ConstraintModel;
class EventModel;
class TimeNodeModel;
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario
template <typename tag, typename impl> class id_base_t;

void updateTimeNodeExtent(
        const Id<TimeNodeModel>& id,
        Scenario::ScenarioModel& s);

// Will call updateTimeNodeExtent
void updateEventExtent(
        const Id<EventModel>& id,
        Scenario::ScenarioModel& s);

// Will call updateEventExtent
void updateConstraintVerticalPos(
        double y,
        const Id<ConstraintModel>& id,
        Scenario::ScenarioModel& s);
