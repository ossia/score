#pragma once
class ConstraintModel;
class EventModel;
class StateModel;
class CommentBlockModel;
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario
#include <iscore/tools/SettableIdentifier.hpp>

namespace StandardRemovalPolicy
{
    void removeConstraint(
            Scenario::ScenarioModel& scenario,
            const Id<ConstraintModel>& constraintId);

    void removeState(
            Scenario::ScenarioModel& scenario,
            StateModel& state);

    void removeComment(
            Scenario::ScenarioModel& scenario,
            CommentBlockModel& cmt);

    void removeEventStatesAndConstraints(
            Scenario::ScenarioModel& scenario,
            const Id<EventModel>& eventId);
}
