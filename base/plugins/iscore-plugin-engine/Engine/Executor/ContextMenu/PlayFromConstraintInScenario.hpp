#pragma once
#include <Scenario/Document/Graph.hpp>
#include <Process/TimeValue.hpp>
#include <hopscotch_set.h>

namespace Engine
{
namespace Execution
{
struct Context;
/**
 * @brief Sets the execution engine to play only the required parts.
 */
struct PlayFromConstraintScenarioPruner
{
  const Scenario::ScenarioInterface& scenar;
  Scenario::ConstraintModel& constraint;
  TimeVal time;

   tsl::hopscotch_set<Scenario::ConstraintModel*> constraintsToKeep() const;

  bool toRemove(
      const tsl::hopscotch_set<Scenario::ConstraintModel*>& toKeep,
      Scenario::ConstraintModel& cst) const;


  void operator()(const Context& exec_ctx);

};

}
}
