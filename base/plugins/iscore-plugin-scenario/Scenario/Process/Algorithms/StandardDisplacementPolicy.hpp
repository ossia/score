#pragma once
#include <Process/TimeValue.hpp>
#include <iscore/model/Identifier.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Scenario/Commands/Scenario/Deletions/ClearInterval.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
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

    // update affected intervals
    for(auto& e : propsToUpdate.intervals)
    {
      auto curIntervalPropertiesToUpdate_id = e.first;

      auto& curIntervalToUpdate
          = scenario.intervals.at(curIntervalPropertiesToUpdate_id);
      auto& curIntervalPropertiesToUpdate = e.second;

      // compute default duration here
      const auto& startDate
          = scenario
                .event(scenario.state(curIntervalToUpdate.startState())
                           .eventId())
                .date();
      const auto& endDate
          = scenario
                .event(
                    scenario.state(curIntervalToUpdate.endState()).eventId())
                .date();

      TimeVal defaultDuration = endDate - startDate;

      // set start date and default duration
      if (!(curIntervalToUpdate.startDate() - startDate).isZero())
      {
        curIntervalToUpdate.setStartDate(startDate);
      }
      curIntervalToUpdate.duration.setDefaultDuration(defaultDuration);

      curIntervalToUpdate.duration.setMinDuration(
          curIntervalPropertiesToUpdate.newMin);
      curIntervalToUpdate.duration.setMaxDuration(
          curIntervalPropertiesToUpdate.newMax);

      for (auto& process : curIntervalToUpdate.processes)
      {
        scaleMethod(process, defaultDuration);
      }

      emit scenario.intervalMoved(curIntervalToUpdate);
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

    // update affected intervals with old values and restor processes
    for(auto& e : propsToUpdate.intervals)
    {
      auto curIntervalPropertiesToUpdate_id = e.first;

      auto& curIntervalToUpdate
          = scenario.intervals.at(curIntervalPropertiesToUpdate_id);
      auto& curIntervalPropertiesToUpdate = e.second;

      // compute default duration here
      const auto& startDate
          = scenario
                .event(scenario.state(curIntervalToUpdate.startState())
                           .eventId())
                .date();
      const auto& endDate
          = scenario
                .event(
                    scenario.state(curIntervalToUpdate.endState()).eventId())
                .date();

      TimeVal defaultDuration = endDate - startDate;

      // set start date and default duration
      if (!(curIntervalToUpdate.startDate() - startDate).isZero())
      {
        curIntervalToUpdate.setStartDate(startDate);
      }
      curIntervalToUpdate.duration.setDefaultDuration(defaultDuration);

      // set durations
      curIntervalToUpdate.duration.setMinDuration(
          curIntervalPropertiesToUpdate.oldMin);
      curIntervalToUpdate.duration.setMaxDuration(
          curIntervalPropertiesToUpdate.oldMax);

      // Now we have to restore the state of each interval that might have
      // been modified
      // during this command.

      // 1. Clear the interval
      // TODO Don't use a command since it serializes a ton of unused stuff.
      Command::ClearInterval clear_cmd{curIntervalToUpdate};
      clear_cmd.redo(ctx);

      // 2. Restore the rackes & processes.
      // Restore the interval. The saving is done in
      // GenericDisplacementPolicy.
      curIntervalPropertiesToUpdate.reload(curIntervalToUpdate);

      emit scenario.intervalMoved(curIntervalToUpdate);
    }
  }
};
}
