#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
class TimeNodeModel;
class EventModel;
namespace Scenario { class ScenarioModel; }
class ConstraintModel;

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
