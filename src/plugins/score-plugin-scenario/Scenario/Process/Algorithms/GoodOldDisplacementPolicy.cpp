// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "GoodOldDisplacementPolicy.hpp"

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Tools/dataStructures.hpp>

#include <score/model/EntityMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/std/Optional.hpp>

#include <QDebug>

template <typename T>
class Reader;

namespace Scenario
{
void GoodOldDisplacementPolicy::computeDisplacement(
    Scenario::ProcessModel& scenario,
    const QVector<Id<TimeSyncModel>>& draggedElements,
    const TimeVal& deltaTime,
    ElementsProperties& elementsProperties)
{
  // this old behavior supports only the move of one timesync
  if (draggedElements.length() != 1)
  {
    qDebug() << "WARNING : computeDisplacement called with empty element list !";
    // move nothing, nothing to undo or redo
    return;
  }
  else
  {
    const Id<TimeSyncModel>& firstTimeSyncMovedId = draggedElements.at(0);
    std::vector<Id<TimeSyncModel>> timeSyncsToTranslate;
    QObjectList processesToSave;

    GoodOldDisplacementPolicy::getRelatedTimeSyncs(
        scenario, firstTimeSyncMovedId, timeSyncsToTranslate);

    // put each concerned timesync in modified elements and compute new values
    for (const auto& curTimeSyncId : timeSyncsToTranslate)
    {
      auto& curTimeSync = scenario.timeSyncs.at(curTimeSyncId);

      // if timesync NOT already in element properties, create new element
      // properties and set the old date
      auto tn_it = elementsProperties.timesyncs.find(curTimeSyncId);
      if (tn_it == elementsProperties.timesyncs.end())
      {
        TimenodeProperties t;
        t.oldDate = curTimeSync.date();
        tn_it = elementsProperties.timesyncs.emplace(curTimeSyncId, std::move(t)).first;
      }

      // put the new date
      auto& val = tn_it.value();
      val.newDate = val.oldDate + deltaTime;
    }

    // Make a list of the intervals that need to be resized
    for (const auto& curTimeSyncId : timeSyncsToTranslate)
    {
      auto& curTimeSync = scenario.timeSync(curTimeSyncId);

      // each previous interval
      for (const auto& ev_id : curTimeSync.events())
      {
        const auto& ev = scenario.event(ev_id);
        for (const auto& st_id : ev.states())
        {
          const auto& st = scenario.states.at(st_id);
          if (const auto& optCurIntervalId = st.previousInterval())
          {
            auto curIntervalId = *optCurIntervalId;
            auto& curInterval = scenario.intervals.at(curIntervalId);
            if (curInterval.graphal())
              continue;
            // if timesync NOT already in element properties, create new
            // element properties and set old values
            auto cur_interval_it = elementsProperties.intervals.find(curIntervalId);
            if (cur_interval_it == elementsProperties.intervals.end())
            {
              IntervalProperties c{curInterval, false};
              c.oldMin = curInterval.duration.minDuration();
              c.oldMax = curInterval.duration.maxDuration();

              cur_interval_it
                  = elementsProperties.intervals.emplace(curIntervalId, std::move(c)).first;

              for (auto& proc : curInterval.processes)
                processesToSave.append(&proc);
            }

            auto& curIntervalStartEvent = Scenario::startEvent(curInterval, scenario);
            auto& startTnodeId = curIntervalStartEvent.timeSync();

            // compute default duration
            TimeVal date;

            // if prev tnode has moved take updated value else take existing
            auto it = elementsProperties.timesyncs.find(startTnodeId);
            if (it != elementsProperties.timesyncs.cend())
            {
              date = it.value().newDate;
            }
            else
            {
              date = curIntervalStartEvent.date();
            }

            const auto& endDate = elementsProperties.timesyncs[curTimeSyncId].newDate;

            TimeVal newDefaultDuration = endDate - date;
            TimeVal deltaBounds = newDefaultDuration - curInterval.duration.defaultDuration();

            auto& val = cur_interval_it.value();
            val.newMin = curInterval.duration.minDuration() + deltaBounds;
            val.newMax = curInterval.duration.maxDuration() + deltaBounds;

            // nothing to do for now
          }
        }
      }
    }

    if (!processesToSave.empty())
    {
      elementsProperties.cables
          = Dataflow::saveCables(processesToSave, score::IDocument::documentContext(scenario));
    }
  }
}

void GoodOldDisplacementPolicy::getRelatedTimeSyncs(
    Scenario::ProcessModel& scenario,
    const Id<TimeSyncModel>& firstTimeSyncMovedId,
    std::vector<Id<TimeSyncModel>>& translatedTimeSyncs)
{
  if (firstTimeSyncMovedId.val() == Scenario::startId_val)
    return;

  auto it
      = std::find(translatedTimeSyncs.begin(), translatedTimeSyncs.end(), firstTimeSyncMovedId);
  if (it == translatedTimeSyncs.end())
  {
    translatedTimeSyncs.push_back(firstTimeSyncMovedId);
  }
  else // timeSync already moved
  {
    return;
  }

  const auto& cur_timeSync = scenario.timeSyncs.at(firstTimeSyncMovedId);
  for (const auto& cur_eventId : cur_timeSync.events())
  {
    const auto& cur_event = scenario.events.at(cur_eventId);

    for (const auto& state_id : cur_event.states())
    {
      const auto& state = scenario.states.at(state_id);
      if (const auto& cons = state.nextInterval())
      {
        const auto& itv = scenario.intervals.at(*cons);
        if (Q_LIKELY(!itv.graphal()))
        {
          const auto& endStateId = itv.endState();
          const auto& endTnId
              = scenario.events.at(scenario.state(endStateId).eventId()).timeSync();
          getRelatedTimeSyncs(scenario, endTnId, translatedTimeSyncs);
        }
      }
    }
  }
}
}
