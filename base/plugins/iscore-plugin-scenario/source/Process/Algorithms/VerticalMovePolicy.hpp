#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
class TimeNodeModel;
class EventModel;
class ScenarioModel;
class ConstraintModel;

void updateTimeNodeExtent(
        const id_type<TimeNodeModel>& id,
        ScenarioModel& s);

// Will call updateTimeNodeExtent
void updateEventExtent(
        const id_type<EventModel>& id,
        ScenarioModel& s);

// Will call updateEventExtent
void updateConstraintVerticalPos(
        double y,
        const id_type<ConstraintModel>& id,
        ScenarioModel& s);
