// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QByteArray>
#include <QMap>
#include <QPair>
#include <iscore/tools/std/Optional.hpp>

#include "GoodOldDisplacementPolicy.hpp"
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Tools/dataStructures.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/Identifier.hpp>

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
    qDebug()
        << "WARNING : computeDisplacement called with empty element list !";
    // move nothing, nothing to undo or redo
    return;
  }
  else
  {
    const Id<TimeSyncModel>& firstTimeSyncMovedId = draggedElements.at(0);
    std::vector<Id<TimeSyncModel>> timeSyncsToTranslate;

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

    // Make a list of the constraints that need to be resized
    for (const auto& curTimeSyncId : timeSyncsToTranslate)
    {
      auto& curTimeSync = scenario.timeSync(curTimeSyncId);

      // each previous constraint
      for (const auto& ev_id : curTimeSync.events())
      {
        const auto& ev = scenario.event(ev_id);
        for (const auto& st_id : ev.states())
        {
          const auto& st = scenario.states.at(st_id);
          if (const auto& optCurConstraintId = st.previousConstraint())
          {
            auto curConstraintId = *optCurConstraintId;
            auto& curConstraint = scenario.constraints.at(curConstraintId);

            // if timesync NOT already in element properties, create new
            // element properties and set old values
            auto cur_constraint_it
                = elementsProperties.constraints.find(curConstraintId);
            if (cur_constraint_it == elementsProperties.constraints.end())
            {
              ConstraintProperties c{curConstraint};
              c.oldMin = curConstraint.duration.minDuration();
              c.oldMax = curConstraint.duration.maxDuration();

              cur_constraint_it
                  = elementsProperties.constraints.emplace(curConstraintId, std::move(c)).first;
            }

            auto& curConstraintStartEvent
                = Scenario::startEvent(curConstraint, scenario);
            auto& startTnodeId = curConstraintStartEvent.timeSync();

            // compute default duration
            TimeVal startDate;

            // if prev tnode has moved take updated value else take existing
            auto it = elementsProperties.timesyncs.find(startTnodeId);
            if (it != elementsProperties.timesyncs.cend())
            {
              startDate = it.value().newDate;
            }
            else
            {
              startDate = curConstraintStartEvent.date();
            }

            const auto& endDate
                = elementsProperties.timesyncs[curTimeSyncId].newDate;

            TimeVal newDefaultDuration = endDate - startDate;
            TimeVal deltaBounds = newDefaultDuration
                                    - curConstraint.duration.defaultDuration();

            auto& val = cur_constraint_it.value();
            val.newMin = curConstraint.duration.minDuration() + deltaBounds;
            val.newMax = curConstraint.duration.maxDuration() + deltaBounds;

            // nothing to do for now
          }
        }
      }
    }
  }
}

void GoodOldDisplacementPolicy::getRelatedTimeSyncs(
    Scenario::ProcessModel& scenario,
    const Id<TimeSyncModel>& firstTimeSyncMovedId,
    std::vector<Id<TimeSyncModel>>& translatedTimeSyncs)
{
  if (firstTimeSyncMovedId.val() == Scenario::startId_val())
    return;

  auto it = std::find(
      translatedTimeSyncs.begin(),
      translatedTimeSyncs.end(),
      firstTimeSyncMovedId);
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
      if (const auto& cons = state.nextConstraint())
      {
        const auto& endStateId = scenario.constraints.at(*cons).endState();
        const auto& endTnId
            = scenario.events.at(scenario.state(endStateId).eventId())
                  .timeSync();
        getRelatedTimeSyncs(scenario, endTnId, translatedTimeSyncs);
      }
    }
  }
}
}
