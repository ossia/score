#pragma once
#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Scenario/Deletions/ClearInterval.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Tools/dataStructures.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/MapCopy.hpp>

namespace Scenario
{
class ProcessModel;
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
    for (auto it = propsToUpdate.timesyncs.cbegin(); it != propsToUpdate.timesyncs.cend(); ++it)
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
    for (auto& e : propsToUpdate.intervals)
    {
      auto curIntervalPropertiesToUpdate_id = e.first;

      auto& curIntervalToUpdate = scenario.intervals.at(curIntervalPropertiesToUpdate_id);
      auto& curIntervalPropertiesToUpdate = e.second;

      // compute default duration here
      const auto& date
          = scenario.event(scenario.state(curIntervalToUpdate.startState()).eventId()).date();
      const auto& endDate
          = scenario.event(scenario.state(curIntervalToUpdate.endState()).eventId()).date();

      TimeVal defaultDuration = endDate - date;

      // set start date and default duration
      using namespace ossia;
      if ((curIntervalToUpdate.date() - date) != 0_tv)
      {
        curIntervalToUpdate.setStartDate(date);
      }
      curIntervalToUpdate.duration.setDefaultDuration(defaultDuration);

      curIntervalToUpdate.duration.setMinDuration(curIntervalPropertiesToUpdate.newMin);
      curIntervalToUpdate.duration.setMaxDuration(curIntervalPropertiesToUpdate.newMax);

      for (auto& process : curIntervalToUpdate.processes)
      {
        scaleMethod(process, defaultDuration);
      }

      scenario.intervalMoved(curIntervalToUpdate);
    }
  }

  template <typename ProcessScaleMethod>
  static void revertPositions(
      const score::DocumentContext& ctx,
      Scenario::ProcessModel& scenario,
      ProcessScaleMethod&& scaleMethod,
      const ElementsProperties& propsToUpdate)
  {
    // update each affected timesyncs with old values
    for (auto it = propsToUpdate.timesyncs.cbegin(); it != propsToUpdate.timesyncs.cend(); ++it)
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
    for (auto& e : propsToUpdate.intervals)
    {
      auto curIntervalPropertiesToUpdate_id = e.first;

      auto& curIntervalToUpdate = scenario.intervals.at(curIntervalPropertiesToUpdate_id);
      const IntervalProperties& curIntervalPropertiesToUpdate = e.second;

      // compute default duration here
      const auto& date
          = scenario.event(scenario.state(curIntervalToUpdate.startState()).eventId()).date();
      const auto& endDate
          = scenario.event(scenario.state(curIntervalToUpdate.endState()).eventId()).date();

      TimeVal defaultDuration = endDate - date;

      // set start date and default duration
      using namespace ossia;
      if ((curIntervalToUpdate.date() - date) != 0_tv)
      {
        curIntervalToUpdate.setStartDate(date);
      }
      curIntervalToUpdate.duration.setDefaultDuration(defaultDuration);

      // set durations
      curIntervalToUpdate.duration.setMinDuration(curIntervalPropertiesToUpdate.oldMin);
      curIntervalToUpdate.duration.setMaxDuration(curIntervalPropertiesToUpdate.oldMax);

      // Now we have to restore the state of each interval that might have
      // been modified
      // during this command.

      // 1. Clear the interval
      {
        curIntervalToUpdate.clearSmallView();

        // We make copies since the iterators might change.
        // TODO check if this is still valid wrt boost::multi_index
        auto processes = shallow_copy(curIntervalToUpdate.processes);
        for (auto process : processes)
        {
          if (!(process->flags() & Process::ProcessFlags::TimeIndependent))
            RemoveProcess(curIntervalToUpdate, process->id());
        }
      }

      // 2. Restore the rackes & processes.
      // Restore the interval. The saving is done in
      // GenericDisplacementPolicy.
      curIntervalPropertiesToUpdate.reload(curIntervalToUpdate);

      scenario.intervalMoved(curIntervalToUpdate);
    }
  }
};
}
