#pragma once
#include <Scenario/Document/Graph.hpp>
#include <Process/TimeValue.hpp>

namespace Engine
{
namespace Execution
{
struct Context;
//MOVEME
/**
 * @brief Sets the execution engine to play only the required parts.
 */
struct PlayFromConstraintScenarioPruner
{
  const Scenario::ScenarioInterface& scenar;
  Scenario::ConstraintModel& constraint;
  TimeValue time;

   tsl::hopscotch_set<Scenario::ConstraintModel*> constraintsToKeep() const;

  bool toRemove(
      const tsl::hopscotch_set<Scenario::ConstraintModel*>& toKeep,
      Scenario::ConstraintModel& cst) const;


  void operator()(const Context& exec_ctx);

};

}
}
