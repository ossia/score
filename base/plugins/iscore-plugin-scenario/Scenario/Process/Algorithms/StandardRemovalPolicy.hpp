#pragma once
#include <iscore/tools/SettableIdentifier.hpp>

namespace Scenario { class ScenarioModel; }
class EventModel;
class ConstraintModel;
class TimeNodeModel;
class StateModel;

namespace StandardRemovalPolicy
{
    void removeConstraint(
            Scenario::ScenarioModel& scenario,
            const Id<ConstraintModel>& constraintId);

    void removeState(
            Scenario::ScenarioModel& scenario,
            StateModel& state);


    void removeEventStatesAndConstraints(
            Scenario::ScenarioModel& scenario,
            const Id<EventModel>& eventId);
}
