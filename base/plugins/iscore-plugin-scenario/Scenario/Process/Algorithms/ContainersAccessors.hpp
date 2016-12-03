#pragma once
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

namespace Scenario
{

inline static auto& getConstraints(const ProcessModel& target)
{
  return target.constraints;
}

inline static auto& getStates(const ProcessModel& target)
{
  return target.states;
}

inline static auto& getEvents(const ProcessModel& target)
{
  return target.events;
}

inline static auto& getTimeNodes(const ProcessModel& target)
{
  return target.timeNodes;
}

inline static auto getConstraints(const BaseScenarioContainer& target)
{
  return target.constraints();
}

inline static auto getStates(const BaseScenarioContainer& target)
{
  return target.states();
}

inline static auto getEvents(const BaseScenarioContainer& target)
{
  return target.events();
}

inline static auto getTimeNodes(const BaseScenarioContainer& target)
{
  return target.timeNodes();
}
}
