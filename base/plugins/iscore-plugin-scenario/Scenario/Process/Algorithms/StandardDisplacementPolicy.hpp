#pragma once
#include <Process/TimeValue.hpp>
#include <iscore/model/Identifier.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Scenario/Commands/Scenario/Deletions/ClearConstraint.hpp>
#include <Scenario/Document/Constraint/Slot.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <iscore/document/DocumentInterface.hpp>

#include <Process/ProcessList.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>

#include <Scenario/Tools/dataStructures.hpp>

namespace Scenario
{
/**
 * @brief The displacementPolicy class
 * This class allows to implement multiple displacement behaviors.
 */
class CommonDisplacementPolicy
{
public:
  template <typename ProcessScaleMethod>
  static void updatePositions(
      Scenario::ProcessModel& scenario,
      ProcessScaleMethod&& scaleMethod,
      const ElementsProperties& propsToUpdate)
  {
    // update each affected timesyncs
    for (auto it = propsToUpdate.timesyncs.cbegin();
         it != propsToUpdate.timesyncs.cend();
         ++it)
    {
      auto& curTimenodeToUpdate = scenario.timeSync(it.key());
      auto& curTimenodePropertiesToUpdate = it.value();

      curTimenodeToUpdate.setDate(curTimenodePropertiesToUpdate.newDate);

      // update related events
      for (const auto& event : curTimenodeToUpdate.events())
      {
        scenario.events.at(event).setDate(curTimenodeToUpdate.date());
      }
    }

    // update affected constraints
    for(auto& e : propsToUpdate.constraints)
    {
      auto curConstraintPropertiesToUpdate_id = e.first;

      auto& curConstraintToUpdate
          = scenario.constraints.at(curConstraintPropertiesToUpdate_id);
      auto& curConstraintPropertiesToUpdate = e.second;

      // compute default duration here
      const auto& startDate
          = scenario
                .event(scenario.state(curConstraintToUpdate.startState())
                           .eventId())
                .date();
      const auto& endDate
          = scenario
                .event(
                    scenario.state(curConstraintToUpdate.endState()).eventId())
                .date();

      TimeVal defaultDuration = endDate - startDate;

      // set start date and default duration
      if (!(curConstraintToUpdate.startDate() - startDate).isZero())
      {
        curConstraintToUpdate.setStartDate(startDate);
      }
      curConstraintToUpdate.duration.setDefaultDuration(defaultDuration);

      curConstraintToUpdate.duration.setMinDuration(
          curConstraintPropertiesToUpdate.newMin);
      curConstraintToUpdate.duration.setMaxDuration(
          curConstraintPropertiesToUpdate.newMax);

      for (auto& process : curConstraintToUpdate.processes)
      {
        scaleMethod(process, defaultDuration);
      }

      emit scenario.constraintMoved(curConstraintToUpdate);
    }
  }

  template <typename ProcessScaleMethod>
  static void revertPositions(
      const iscore::DocumentContext& ctx,
      Scenario::ProcessModel& scenario,
      ProcessScaleMethod&& scaleMethod,
      const ElementsProperties& propsToUpdate)
  {
    // update each affected timesyncs with old values
    for (auto it = propsToUpdate.timesyncs.cbegin();
         it != propsToUpdate.timesyncs.cend();
         ++it)
    {
      auto& curTimenodeToUpdate = scenario.timeSync(it.key());
      auto& curTimenodePropertiesToUpdate = it.value();

      curTimenodeToUpdate.setDate(curTimenodePropertiesToUpdate.oldDate);

      // update related events to mach the date
      for (const auto& event : curTimenodeToUpdate.events())
      {
        scenario.events.at(event).setDate(curTimenodeToUpdate.date());
      }
    }

    // update affected constraints with old values and restor processes
    for(auto& e : propsToUpdate.constraints)
    {
      auto curConstraintPropertiesToUpdate_id = e.first;

      auto& curConstraintToUpdate
          = scenario.constraints.at(curConstraintPropertiesToUpdate_id);
      auto& curConstraintPropertiesToUpdate = e.second;

      // compute default duration here
      const auto& startDate
          = scenario
                .event(scenario.state(curConstraintToUpdate.startState())
                           .eventId())
                .date();
      const auto& endDate
          = scenario
                .event(
                    scenario.state(curConstraintToUpdate.endState()).eventId())
                .date();

      TimeVal defaultDuration = endDate - startDate;

      // set start date and default duration
      if (!(curConstraintToUpdate.startDate() - startDate).isZero())
      {
        curConstraintToUpdate.setStartDate(startDate);
      }
      curConstraintToUpdate.duration.setDefaultDuration(defaultDuration);

      // set durations
      curConstraintToUpdate.duration.setMinDuration(
          curConstraintPropertiesToUpdate.oldMin);
      curConstraintToUpdate.duration.setMaxDuration(
          curConstraintPropertiesToUpdate.oldMax);

      // Now we have to restore the state of each constraint that might have
      // been modified
      // during this command.

      // 1. Clear the constraint
      // TODO Don't use a command since it serializes a ton of unused stuff.
      Command::ClearConstraint clear_cmd{curConstraintToUpdate};
      clear_cmd.redo(ctx);

      // 2. Restore the rackes & processes.
      // Restore the constraint. The saving is done in
      // GenericDisplacementPolicy.
      curConstraintPropertiesToUpdate.reload(curConstraintToUpdate);

      emit scenario.constraintMoved(curConstraintToUpdate);
    }
  }
};
}
