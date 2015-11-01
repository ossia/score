#pragma once
#include <iscore/tools/SettableIdentifier.hpp>

class ScenarioModel;
class EventModel;
class ConstraintModel;
class TimeNodeModel;
class StateModel;

namespace StandardRemovalPolicy
{
    void removeConstraint(
            ScenarioModel& scenario,
            const Id<ConstraintModel>& constraintId);

    void removeState(
            ScenarioModel& scenario,
            StateModel& state);


    void removeEventStatesAndConstraints(
            ScenarioModel& scenario,
            const Id<EventModel>& eventId);
}
