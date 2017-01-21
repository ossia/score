#pragma once

#include <Process/TimeValue.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
namespace Scenario
{
inline TimeVal
getDate(const Scenario::ProcessModel& scenario, const Id<StateModel>& state)
{
  return scenario.timeNodes
      .at(scenario.events.at(scenario.states.at(state).eventId()).timeNode())
      .date();
}

inline TimeVal
getDate(const Scenario::ProcessModel& scenario, const Id<EventModel>& event)
{
  return scenario.timeNodes.at(scenario.events.at(event).timeNode()).date();
}

inline TimeVal getDate(
    const Scenario::ProcessModel& scenario, const Id<TimeNodeModel>& timenode)
{
  return scenario.timeNodes.at(timenode).date();
}
}
