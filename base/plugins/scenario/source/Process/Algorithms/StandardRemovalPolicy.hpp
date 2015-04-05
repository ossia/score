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

    void removeEventAndConstraints(ScenarioModel& scenario,
                     id_type<EventModel> eventId);
}
