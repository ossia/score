#pragma once

#include <Process/TimeValue.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
namespace Scenario
{
inline TimeVal getDate(const Scenario::ProcessModel& scenario, const Id<StateModel>& state)
{
  return scenario.timeSyncs.at(scenario.events.at(scenario.states.at(state).eventId()).timeSync())
      .date();
}

inline TimeVal getDate(const Scenario::ProcessModel& scenario, const Id<EventModel>& event)
{
  return scenario.timeSyncs.at(scenario.events.at(event).timeSync()).date();
}

inline TimeVal getDate(const Scenario::ProcessModel& scenario, const Id<TimeSyncModel>& timesync)
{
  return scenario.timeSyncs.at(timesync).date();
}
}
