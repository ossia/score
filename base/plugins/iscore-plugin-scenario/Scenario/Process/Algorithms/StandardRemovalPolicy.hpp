#pragma once
class ConstraintModel;
class EventModel;
class StateModel;
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario
template <typename tag, typename impl> class id_base_t;

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
