// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioPasteElements.hpp"

#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/model/path/ObjectPath.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateModel.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/MapSerialization.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/Clamp.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/detail/algorithms.hpp>

#include <Scenario/Commands/Scenario/ScenarioPaste.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <cstddef>
#include <limits>
#include <vector>

#include <unordered_map>
namespace Scenario
{
std::vector<Process::CableData>
cableDataFromCablesJson(const rapidjson::Document::ConstArray& arr)
{
  std::vector<Process::CableData> cables;

  cables.reserve(arr.Size());
  for (const auto& element : arr)
  {
    Process::CableData cd;
    if (element.IsObject() && element.HasMember("ObjectName"))
    {
      cd <<= JsonValue{element};
      cables.emplace_back(std::move(cd));
    }
    else if (element.IsArray())
    {
      cd <<= JsonValue{element.GetArray()[1]};
      cables.emplace_back(cd);
    }
  }

  return cables;
}
std::vector<Process::CableData>
cableDataFromCablesJson(const rapidjson::Document::Array& arr)
{
  std::vector<Process::CableData> cables;

  cables.reserve(arr.Size());
  for (const auto& element : arr)
  {
    Process::CableData cd;
    if (element.IsObject() && element.HasMember("ObjectName"))
    {
      cd <<= JsonValue{element};
      cables.emplace_back(std::move(cd));
    }
    else if (element.IsArray())
    {
      cd <<= JsonValue{element.GetArray()[1]};
    cables.emplace_back(cd);
  }
}

return cables;
}

namespace Command
{
ScenarioPasteElements::ScenarioPasteElements(
    const Scenario::ProcessModel& scenario,
    const rapidjson::Value& obj,
    const Scenario::Point& pt)
    : m_ts{scenario}
{
  auto& ctx = score::IDocument::documentContext(scenario);
  auto
      [timesyncs,
       intervals,
       events,
       states,
       cables,
       interval_ids,
       timesync_ids,
       event_ids,
       state_ids]
      = ScenarioBeingCopied{obj, scenario, ctx};

  // We set the new ids everywhere
  {
    int i = 0;
    for (TimeSyncModel* timesync : timesyncs)
    {
      for (EventModel* event : events)
      {
        if (event->timeSync() == timesync->id())
        {
          event->changeTimeSync(timesync_ids[i]);
        }
      }

      timesync->setId(timesync_ids[i]);
      i++;
    }
  }

  {
    int i = 0;
    for (EventModel* event : events)
    {
      {
        auto it = std::find_if(
            timesyncs.begin(), timesyncs.end(), [&](TimeSyncModel* tn) {
              return tn->id() == event->timeSync();
            });
        SCORE_ASSERT(it != timesyncs.end());
        auto timesync = *it;
        timesync->removeEvent(event->id());
        timesync->addEvent(event_ids[i]);
      }

      for (StateModel* state : states)
      {
        if (state->eventId() == event->id())
        {
          state->setEventId(event_ids[i]);
        }
      }

      event->setId(event_ids[i]);
      i++;
    }
  }

  {
    int i = 0;
    for (StateModel* state : states)
    {
      {
        auto it = std::find_if(
            events.begin(), events.end(), [&](EventModel* event) {
              return event->id() == state->eventId();
            });
        SCORE_ASSERT(it != events.end());
        auto event = *it;
        event->removeState(state->id());
        event->addState(state_ids[i]);
      }

      for (IntervalModel* interval : intervals)
      {
        if (interval->startState() == state->id())
          interval->setStartState(state_ids[i]);
        else if (interval->endState() == state->id())
          interval->setEndState(state_ids[i]);
      }

      state->setId(state_ids[i]);
      i++;
    }
  }

  // Cables //
  m_cables.cables = mapCopiedCables(ctx, cables, intervals, interval_ids, scenario);

  {
    int i = 0;
    for (IntervalModel* interval : intervals)
    {
      interval->setId(interval_ids[i]);
      {
        auto start_state_id = ossia::find_if(states, [&](auto state) {
          return state->id() == interval->startState();
        });
        if (start_state_id != states.end())
          SetNextInterval(**start_state_id, *interval);
      }
      {
        auto end_state_id = ossia::find_if(states, [&](auto state) {
          return state->id() == interval->endState();
        });
        if (end_state_id != states.end())
          SetPreviousInterval(**end_state_id, *interval);
      }

      const auto& fv = interval->fullView();
      if (!fv.empty() && interval->smallView().empty())
      {
        const auto N = fv.size();
        for (std::size_t i = 0; i < N; i++)
        {
          interval->addSlot(Slot{{fv[i].process}, fv[i].process}, i);
        }
      }
      i++;
    }
  }

  // Set the correct positions / dates.
  // Take the earliest interval or timesync and compute the delta; apply the
  // delta everywhere.
  if (!intervals.empty() || !timesyncs.empty()) // Should always be the case.
  {
    auto earliestTime = !intervals.empty() ? intervals.front()->date()
                                           : timesyncs.front()->date();
    for (const IntervalModel* interval : intervals)
    {
      const auto& t = interval->date();
      if (t < earliestTime)
        earliestTime = t;
    }
    for (const TimeSyncModel* tn : timesyncs)
    {
      const auto& t = tn->date();
      if (t < earliestTime)
        earliestTime = t;
    }
    for (const EventModel* ev : events)
    {
      const auto& t = ev->date();
      if (t < earliestTime)
        earliestTime = t;
    }

    auto delta_t = pt.date - earliestTime;
    for (IntervalModel* interval : intervals)
    {
      interval->setStartDate(interval->date() + delta_t);
    }
    for (TimeSyncModel* tn : timesyncs)
    {
      tn->setDate(tn->date() + delta_t);
    }
    for (EventModel* ev : events)
    {
      ev->setDate(ev->date() + delta_t);
    }
  }

  // Same for y.
  // Note : due to the coordinates system, the highest y is the one closest to
  // 0.
  auto highest_y = std::numeric_limits<double>::max();
  for (const StateModel* state : states)
  {
    auto y = state->heightPercentage();
    if (y < highest_y)
    {
      highest_y = y;
    }
  }

  auto delta_y = pt.y - highest_y;

  for (IntervalModel* cst : intervals)
  {
    cst->setHeightPercentage(clamp(cst->heightPercentage() + delta_y, 0., 1.));
  }
  for (StateModel* state : states)
  {
    state->setHeightPercentage(
        clamp(state->heightPercentage() + delta_y, 0., 1.));
  }

  // We reserialize here in order to not have dangling pointers and bad cache
  // in the ids.
  m_ids_intervals.reserve(intervals.size());
  m_json_intervals.reserve(intervals.size());
  for (auto elt : intervals)
  {
    m_ids_intervals.push_back(elt->id());
    m_json_intervals.push_back(score::marshall<DataStream>(*elt));

    delete elt;
  }

  m_ids_timesyncs.reserve(timesyncs.size());
  m_json_timesyncs.reserve(timesyncs.size());
  for (auto elt : timesyncs)
  {
    m_ids_timesyncs.push_back(elt->id());
    m_json_timesyncs.push_back(score::marshall<DataStream>(*elt));

    delete elt;
  }

  m_json_events.reserve(events.size());
  m_ids_events.reserve(events.size());
  for (auto elt : events)
  {
    m_ids_events.push_back(elt->id());
    m_json_events.push_back(score::marshall<DataStream>(*elt));

    delete elt;
  }

  m_json_states.reserve(states.size());
  m_ids_states.reserve(states.size());
  for (auto elt : states)
  {
    m_ids_states.push_back(elt->id());
    m_json_states.push_back(score::marshall<DataStream>(*elt));

    delete elt;
  }
}

void ScenarioPasteElements::undo(const score::DocumentContext& ctx) const
{
  auto& scenario = m_ts.find(ctx);

  m_cables.undo(ctx);
  for (const auto& elt : m_ids_intervals)
  {
    ScenarioCreate<IntervalModel>::undo(elt, scenario);
  }
  for (const auto& elt : m_ids_states)
  {
    ScenarioCreate<StateModel>::undo(elt, scenario);
  }
  for (const auto& elt : m_ids_events)
  {
    ScenarioCreate<EventModel>::undo(elt, scenario);
  }
  for (const auto& elt : m_ids_timesyncs)
  {
    ScenarioCreate<TimeSyncModel>::undo(elt, scenario);
  }
}

void ScenarioPasteElements::redo(const score::DocumentContext& ctx) const
{
  Scenario::ProcessModel& scenario = m_ts.find(ctx);

  std::vector<TimeSyncModel*> addedTimeSyncs;
  addedTimeSyncs.reserve(m_json_timesyncs.size());
  std::vector<EventModel*> addedEvents;
  addedEvents.reserve(m_json_events.size());
  for (const auto& timesync : m_json_timesyncs)
  {
    auto tn = new TimeSyncModel(DataStream::Deserializer{timesync}, &scenario);
    scenario.timeSyncs.add(tn);
    addedTimeSyncs.push_back(tn);
  }

  for (const auto& event : m_json_events)
  {
    auto ev = new EventModel(DataStream::Deserializer{event}, &scenario);
    scenario.events.add(ev);
    addedEvents.push_back(ev);
  }

  for (const auto& state : m_json_states)
  {
    scenario.states.add(new StateModel(
        DataStream::Deserializer{state}, scenario.context(), &scenario));
  }

  for (const auto& interval : m_json_intervals)
  {
    auto cst = new IntervalModel(
        DataStream::Deserializer{interval}, scenario.context(), &scenario);
    scenario.intervals.add(cst);
  }

  m_cables.redo(ctx);
}

void ScenarioPasteElements::serializeImpl(DataStreamInput& s) const
{
  s << m_ts << m_ids_timesyncs << m_ids_events << m_ids_states
    << m_ids_intervals << m_json_timesyncs << m_json_events << m_json_states
    << m_json_intervals << m_cables.cables;
}

void ScenarioPasteElements::deserializeImpl(DataStreamOutput& s)
{
  s >> m_ts >> m_ids_timesyncs >> m_ids_events >> m_ids_states
      >> m_ids_intervals >> m_json_timesyncs >> m_json_events >> m_json_states
      >> m_json_intervals >> m_cables.cables;
}
}
}
