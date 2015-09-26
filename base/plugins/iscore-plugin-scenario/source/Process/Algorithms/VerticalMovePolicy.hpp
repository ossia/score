#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
class TimeNodeModel;
class EventModel;
class ScenarioModel;
class ConstraintModel;

void updateTimeNodeExtent(
        const Id<TimeNodeModel>& id,
        ScenarioModel& s);

// Will call updateTimeNodeExtent
void updateEventExtent(
        const Id<EventModel>& id,
        ScenarioModel& s);

// Will call updateEventExtent
void updateConstraintVerticalPos(
        double y,
        const Id<ConstraintModel>& id,
        ScenarioModel& s);
