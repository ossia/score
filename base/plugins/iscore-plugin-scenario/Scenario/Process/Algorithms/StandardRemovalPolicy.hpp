#pragma once
#include <iscore/tools/SettableIdentifier.hpp>

namespace Scenario
{
class ConstraintModel;
class EventModel;
class StateModel;
class ProcessModel;
class CommentBlockModel;
namespace StandardRemovalPolicy
{
void removeConstraint(
    Scenario::ProcessModel& scenario, const Id<ConstraintModel>& constraintId);

void removeState(Scenario::ProcessModel& scenario, StateModel& state);

void removeComment(Scenario::ProcessModel& scenario, CommentBlockModel& cmt);

void removeEventStatesAndConstraints(
    Scenario::ProcessModel& scenario, const Id<EventModel>& eventId);
}
}
