#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
class TimeNodeModel;
class EventModel;
class ScenarioModel;
class ConstraintModel;

void updateTimeNodeExtent(
        const id_type<TimeNodeModel>& id,
        ScenarioModel& s);

void updateEventExtent(
        const id_type<EventModel>& id,
        ScenarioModel& s);

void updateConstraintVerticalPos(
        double y,
        const id_type<ConstraintModel>& id,
        ScenarioModel& s);
