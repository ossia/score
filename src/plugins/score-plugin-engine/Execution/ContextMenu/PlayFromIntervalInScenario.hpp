#pragma once
#include <Process/TimeValue.hpp>

#include <Scenario/Document/Graph.hpp>

#include <ossia/detail/hash_map.hpp>

namespace Execution
{
struct Context;
class BaseScenarioElement;
/**
 * @brief Sets the execution engine to play only the required parts.
 */
struct PlayFromIntervalScenarioPruner
{
  const Scenario::ScenarioInterface& scenar;
  Scenario::IntervalModel& interval;
  TimeVal time;

  ossia::hash_set<Scenario::IntervalModel*> intervalsToKeep() const;

  bool toRemove(
      const ossia::hash_set<Scenario::IntervalModel*>& toKeep,
      Scenario::IntervalModel& cst) const;

  void operator()(const Context& exec_ctx, const BaseScenarioElement& bs);
};
}
