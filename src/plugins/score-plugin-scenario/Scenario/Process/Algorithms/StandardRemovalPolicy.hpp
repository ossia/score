#pragma once
#include <score/model/Identifier.hpp>

namespace Scenario
{
class IntervalModel;
class EventModel;
class StateModel;
class ProcessModel;
class CommentBlockModel;
namespace StandardRemovalPolicy
{
void removeInterval(Scenario::ProcessModel& scenario, const Id<IntervalModel>& intervalId);

void removeState(Scenario::ProcessModel& scenario, StateModel& state);

void removeComment(Scenario::ProcessModel& scenario, CommentBlockModel& cmt);

void removeEventStatesAndIntervals(
    Scenario::ProcessModel& scenario,
    const Id<EventModel>& eventId);
}
}
