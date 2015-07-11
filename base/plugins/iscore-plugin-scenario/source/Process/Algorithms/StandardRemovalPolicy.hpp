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
            const id_type<ConstraintModel>& constraintId);

    void removeState(
            ScenarioModel& scenario,
            const id_type<StateModel>& stateId);


    void removeEventStatesAndConstraints(
            ScenarioModel& scenario,
            const id_type<EventModel>& eventId);
}
