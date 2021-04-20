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
#include <Scenario/Process/Algorithms/Accessors.hpp>
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
        scenario.events.at(event).setDate(curTimenodePropertiesToUpdate.newDate);
      }
    }

    // update affected intervals
    for (auto& e : propsToUpdate.intervals)
    {
      auto curIntervalPropertiesToUpdate_id = e.first;

      auto& curInterval = scenario.intervals.at(curIntervalPropertiesToUpdate_id);
      auto& curIntervalPropertiesToUpdate = e.second;

      // compute default duration here
      const auto& date = Scenario::startEvent(curInterval, scenario).date();
      const auto& endDate = Scenario::endEvent(curInterval, scenario).date();

      TimeVal defaultDuration = endDate - date;

      // set start date and default duration
      using namespace ossia;
      if (curInterval.date() != date)
      {
        curInterval.setStartDate(date);
      }
      curInterval.duration.setDefaultDuration(defaultDuration);

      curInterval.duration.setMinDuration(curIntervalPropertiesToUpdate.newMin);
      curInterval.duration.setMaxDuration(curIntervalPropertiesToUpdate.newMax);

      for (auto& process : curInterval.processes)
      {
        scaleMethod(process, defaultDuration);
      }

      scenario.intervalMoved(&curInterval);
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
        scenario.events.at(event).setDate(curTimenodePropertiesToUpdate.oldDate);
      }
    }

    // update affected intervals with old values and restor processes
    for (auto& e : propsToUpdate.intervals)
    {
      auto curIntervalPropertiesToUpdate_id = e.first;

      auto& curInterval = scenario.intervals.at(curIntervalPropertiesToUpdate_id);
      const IntervalProperties& curIntervalPropertiesToUpdate = e.second;

      // compute default duration here
      const auto& date = Scenario::startEvent(curInterval, scenario).date();
      const auto& endDate = Scenario::endEvent(curInterval, scenario).date();

      TimeVal defaultDuration = endDate - date;

      SCORE_ASSERT(defaultDuration == curIntervalPropertiesToUpdate.oldDefault);

      // set start date and default duration
      using namespace ossia;
      if (curInterval.date() != curIntervalPropertiesToUpdate.oldDate)
      {
        curInterval.setStartDate(curIntervalPropertiesToUpdate.oldDate);
      }
      curInterval.duration.setDefaultDuration(curIntervalPropertiesToUpdate.oldDefault);

      // set durations
      curInterval.duration.setMinDuration(curIntervalPropertiesToUpdate.oldMin);
      curInterval.duration.setMaxDuration(curIntervalPropertiesToUpdate.oldMax);

      // Now we have to restore the state of each interval that might have
      // been modified
      // during this command.

      // 1. Clear the interval
      {
        curInterval.clearSmallView();

        // We make copies since the iterators might change.
        // TODO check if this is still valid wrt boost::multi_index
        auto processes = shallow_copy(curInterval.processes);
        for (auto process : processes)
        {
          if (!(process->flags() & Process::ProcessFlags::TimeIndependent))
            RemoveProcess(curInterval, process->id());
        }
      }

      // 2. Restore the rackes & processes.
      // Restore the interval. The saving is done in
      // GenericDisplacementPolicy.
      curIntervalPropertiesToUpdate.reload(curInterval);

      scenario.intervalMoved(&curInterval);
    }
  }
};
}
