#pragma once
#include <iscore/tools/SettableIdentifier.hpp>

namespace Scenario
{
class ConstraintModel;
class EventModel;
class StateModel;
class ScenarioModel;
class CommentBlockModel;
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
}
