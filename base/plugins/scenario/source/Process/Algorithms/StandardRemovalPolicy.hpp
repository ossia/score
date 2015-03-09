#pragma once
#include <iscore/tools/SettableIdentifier.hpp>

class ScenarioModel;
class EventModel;
class ConstraintModel;
class TimeNodeModel;

namespace StandardRemovalPolicy
{
    void removeConstraint(ScenarioModel& scenario,
                          id_type<ConstraintModel> constraintId);

    void removeEvent(ScenarioModel& scenario,
                     id_type<EventModel> eventId);

    void removeTimeNode(ScenarioModel& scenario,
                        id_type<TimeNodeModel> timeNodeId);
}
