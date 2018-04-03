// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <QDataStream>
#include <QHash>
#include <QJsonArray>
#include <QJsonValue>
#include <QtGlobal>
#include <algorithm>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <cstddef>
#include <score/tools/Clamp.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <iterator>
#include <limits>
#include <vector>

#include "ScenarioPasteElements.hpp"
#include <ossia/detail/algorithms.hpp>
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <core/document/Document.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/path/ObjectPath.hpp>
// Needed for copy since we want to generate IDs that are neither
// in the scenario in which we are copying into, nor in the elements
// that we copied because it may cause conflicts.
// MOVEME
template <typename T, typename Vector1, typename Vector2>
static auto getStrongIdRange2(
    std::size_t s, const Vector1& existing1, const Vector2& existing2)
{
  std::vector<Id<T>> vec;
  vec.reserve(s + existing1.size() + existing2.size());
  std::transform(
      existing1.begin(), existing1.end(), std::back_inserter(vec),
      [](const auto& elt) { return elt.id(); });
  std::transform(
      existing2.begin(), existing2.end(), std::back_inserter(vec),
      [](const auto& elt) { return elt->id(); });

  for (; s-- > 0;)
  {
    vec.push_back(getStrongId(vec));
  }
  auto final = std::vector<Id<T>>(
      vec.begin() + existing1.size() + existing2.size(), vec.end());

  return final;
}

template <typename T, typename Vector>
auto getStrongIdRangePtr(std::size_t s, const Vector& existing)
{
  std::vector<Id<T>> vec;
  vec.reserve(s + existing.size());
  std::transform(
      existing.begin(), existing.end(), std::back_inserter(vec),
      [](const auto& elt) { return elt->id(); });

  for (; s-- > 0;)
  {
    vec.push_back(getStrongId(vec));
  }

  return std::vector<Id<T>>(vec.begin() + existing.size(), vec.end());
}

namespace Scenario
{
namespace Command
{
ScenarioPasteElements::ScenarioPasteElements(
    const Scenario::ProcessModel& scenario,
    const QJsonObject& obj,
    const Scenario::Point& pt)
    : m_ts{scenario}
{
  // We assign new ids WRT the elements of the scenario - these ids can
  // be easily mapped.
  auto& stack = score::IDocument::documentContext(scenario).commandStack;

  std::vector<TimeSyncModel*> timesyncs;
  std::vector<IntervalModel*> intervals;
  std::vector<EventModel*> events;
  std::vector<StateModel*> states;

  // TODO this is really a bad idea... either they should be properly added, or
  // the json should be modified without including anything in the scenario.
  // Especially their parents aren't coherent (TimeSync must not have a parent
  // because it tries to access the event in the scenario if it has one and
  // Interval needs a parent for the RelativePath in LayerModel)
  // We deserialize everything
  {
    const auto json_arr = obj["Intervals"].toArray();
    intervals.reserve(json_arr.size());
    for (const auto& element : json_arr)
    {
      intervals.emplace_back(new IntervalModel{
          JSONObject::Deserializer{element.toObject()}, (QObject*)&scenario});
    }
  }
  {
    const auto json_arr = obj["TimeNodes"].toArray();
    timesyncs.reserve(json_arr.size());
    for (const auto& element : json_arr)
    {
      timesyncs.emplace_back(new TimeSyncModel{
          JSONObject::Deserializer{element.toObject()}, nullptr});
    }
  }
  {
    const auto json_arr = obj["Events"].toArray();
    events.reserve(json_arr.size());
    for (const auto& element : json_arr)
    {
      events.emplace_back(new EventModel{
          JSONObject::Deserializer{element.toObject()}, nullptr});
    }
  }
  {
    const auto json_arr = obj["States"].toArray();
    states.reserve(json_arr.size());
    for (const auto& element : json_arr)
    {
      states.emplace_back(new StateModel{
          JSONObject::Deserializer{element.toObject()}, stack, nullptr});
    }
  }

  // We generate identifiers for the forthcoming elements
  auto interval_ids = getStrongIdRange2<IntervalModel>(
      intervals.size(), scenario.intervals, intervals);
  auto timesync_ids = getStrongIdRange2<TimeSyncModel>(
      timesyncs.size(), scenario.timeSyncs, timesyncs);
  auto event_ids
      = getStrongIdRange2<EventModel>(events.size(), scenario.events, events);
  auto state_ids
      = getStrongIdRange2<StateModel>(states.size(), scenario.states, states);

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
      if(!fv.empty() && interval->smallView().empty())
      {
        const auto N = fv.size();
        for(std::size_t i = 0; i < N; i++)
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
    m_json_intervals.push_back(score::marshall<JSONObject>(*elt));

    delete elt;
  }

  m_ids_timesyncs.reserve(timesyncs.size());
  m_json_timesyncs.reserve(timesyncs.size());
  for (auto elt : timesyncs)
  {
    m_ids_timesyncs.push_back(elt->id());
    m_json_timesyncs.push_back(score::marshall<JSONObject>(*elt));

    delete elt;
  }

  m_json_events.reserve(events.size());
  m_ids_events.reserve(events.size());
  for (auto elt : events)
  {
    m_ids_events.push_back(elt->id());
    m_json_events.push_back(score::marshall<JSONObject>(*elt));

    delete elt;
  }

  m_json_states.reserve(states.size());
  m_ids_states.reserve(states.size());
  for (auto elt : states)
  {
    m_ids_states.push_back(elt->id());
    m_json_states.push_back(score::marshall<JSONObject>(*elt));

    delete elt;
  }
}



void ScenarioPasteElements::undo(const score::DocumentContext& ctx) const
{
  auto& scenario = m_ts.find(ctx);

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
    auto tn = new TimeSyncModel(JSONObject::Deserializer{timesync}, &scenario);
    scenario.timeSyncs.add(tn);
    addedTimeSyncs.push_back(tn);
  }

  for (const auto& event : m_json_events)
  {
    auto ev = new EventModel(JSONObject::Deserializer{event}, &scenario);
    scenario.events.add(ev);
    addedEvents.push_back(ev);
  }

  auto& stack = score::IDocument::documentContext(scenario).commandStack;
  for (const auto& state : m_json_states)
  {
    scenario.states.add(
        new StateModel(JSONObject::Deserializer{state}, stack, &scenario));
  }

  for (const auto& interval : m_json_intervals)
  {
    auto cst
        = new IntervalModel(JSONObject::Deserializer{interval}, &scenario);
    scenario.intervals.add(cst);
  }

  for (const auto& event : addedEvents)
  {
    updateEventExtent(event->id(), scenario);
  }
  for (const auto& timesync : addedTimeSyncs)
  {
    updateTimeSyncExtent(timesync->id(), scenario);
  }
}

void ScenarioPasteElements::serializeImpl(DataStreamInput& s) const
{
  s << m_ts
    << m_ids_timesyncs << m_ids_events << m_ids_states << m_ids_intervals
    << m_json_timesyncs << m_json_events << m_json_states << m_json_intervals ;
}

void ScenarioPasteElements::deserializeImpl(DataStreamOutput& s)
{
  s >> m_ts
      >> m_ids_timesyncs >> m_ids_events >> m_ids_states >> m_ids_intervals
      >> m_json_timesyncs >> m_json_events >> m_json_states
      >> m_json_intervals;
}










ScenarioPasteElementsAfter::ScenarioPasteElementsAfter(
    const Scenario::ProcessModel& scenario,
    const QJsonObject& obj)
    : m_ts{scenario}
{
  const Scenario::TimeSyncModel* attach_sync = furthestHierarchicallySelectedTimeSync(scenario);
  m_attachSync = attach_sync->id();

  // We assign new ids WRT the elements of the scenario - these ids can
  // be easily mapped.
  auto& stack = score::IDocument::documentContext(scenario).commandStack;

  std::vector<TimeSyncModel*> timesyncs;
  std::vector<IntervalModel*> intervals;
  std::vector<EventModel*> events;
  std::vector<StateModel*> states;

  // TODO this is really a bad idea... either they should be properly added, or
  // the json should be modified without including anything in the scenario.
  // Especially their parents aren't coherent (TimeSync must not have a parent
  // because it tries to access the event in the scenario if it has one and
  // Interval needs a parent for the RelativePath in LayerModel)
  // We deserialize everything
  {
    const auto json_arr = obj["Intervals"].toArray();
    intervals.reserve(json_arr.size());
    for (const auto& element : json_arr)
    {
      intervals.emplace_back(new IntervalModel{
          JSONObject::Deserializer{element.toObject()}, (QObject*)&scenario});
    }
  }
  {
    const auto json_arr = obj["TimeNodes"].toArray();
    timesyncs.reserve(json_arr.size());
    for (const auto& element : json_arr)
    {
      timesyncs.emplace_back(new TimeSyncModel{
          JSONObject::Deserializer{element.toObject()}, nullptr});
    }
  }
  {
    const auto json_arr = obj["Events"].toArray();
    events.reserve(json_arr.size());
    for (const auto& element : json_arr)
    {
      events.emplace_back(new EventModel{
          JSONObject::Deserializer{element.toObject()}, nullptr});
    }
  }
  {
    const auto json_arr = obj["States"].toArray();
    states.reserve(json_arr.size());
    for (const auto& element : json_arr)
    {
      states.emplace_back(new StateModel{
          JSONObject::Deserializer{element.toObject()}, stack, nullptr});
    }
  }

  // We generate identifiers for the forthcoming elements
  auto interval_ids = getStrongIdRange2<IntervalModel>(
      intervals.size(), scenario.intervals, intervals);
  auto timesync_ids = getStrongIdRange2<TimeSyncModel>(
      timesyncs.size(), scenario.timeSyncs, timesyncs);
  auto event_ids
      = getStrongIdRange2<EventModel>(events.size(), scenario.events, events);
  auto state_ids
      = getStrongIdRange2<StateModel>(states.size(), scenario.states, states);

  // We set the new ids everywhere
  {
    int i = 0;
    for (auto it = timesyncs.begin(); it != timesyncs.end(); )
    {
      auto timesync = *it;
      bool no_prev = true;
      for (EventModel* event : events)
      {
        if (event->timeSync() == timesync->id())
        {
          for(StateModel* st : states)
          {
            if(st->eventId() == event->id())
            {
              // Note: we do not use the previous / next interval methds
              // of StateModel since they are set to none (see ScenarioCopy.cpp: copySelected)
              for(IntervalModel* itv : intervals)
              {
                if(itv->endState() == st->id())
                {
                  no_prev = false;
                }
              }
            }
          }
        }
      }

      if(no_prev)
      {
        // no previous intervals: we attach to the selected timesync
        for (EventModel* event : events)
        {
          if (event->timeSync() == timesync->id())
          {
            event->changeTimeSync(attach_sync->id());
          }
        }
        delete timesync;
        it = timesyncs.erase(it);
      }
      else
      {
        // previous intervals: we don't touch it
        for (EventModel* event : events)
        {
          if (event->timeSync() == timesync->id())
          {
            event->changeTimeSync(timesync_ids[i]);
          }
        }
        timesync->setId(timesync_ids[i]);
        ++it;
      }

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
        if(it != timesyncs.end())
        {
          auto timesync = *it;
          timesync->removeEvent(event->id());
          timesync->addEvent(event_ids[i]);
        }
        else
        {
          SCORE_ASSERT(event->timeSync() == attach_sync->id());
          m_eventsToAttach.push_back(event_ids[i]);
        }
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
      if(!fv.empty() && interval->smallView().empty())
      {
        const auto N = fv.size();
        for(std::size_t i = 0; i < N; i++)
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

    auto delta_t = attach_sync->date() - earliestTime;
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

  auto delta_y = attach_sync->extent().bottom() - highest_y;

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
    m_json_intervals.push_back(score::marshall<JSONObject>(*elt));

    delete elt;
  }

  m_ids_timesyncs.reserve(timesyncs.size());
  m_json_timesyncs.reserve(timesyncs.size());
  for (auto elt : timesyncs)
  {
    m_ids_timesyncs.push_back(elt->id());
    m_json_timesyncs.push_back(score::marshall<JSONObject>(*elt));

    delete elt;
  }

  m_json_events.reserve(events.size());
  m_ids_events.reserve(events.size());
  for (auto elt : events)
  {
    m_ids_events.push_back(elt->id());
    m_json_events.push_back(score::marshall<JSONObject>(*elt));

    delete elt;
  }

  m_json_states.reserve(states.size());
  m_ids_states.reserve(states.size());
  for (auto elt : states)
  {
    m_ids_states.push_back(elt->id());
    m_json_states.push_back(score::marshall<JSONObject>(*elt));

    delete elt;
  }
}

void ScenarioPasteElementsAfter::undo(const score::DocumentContext& ctx) const
{
  // TODO remove added events
  auto& scenario = m_ts.find(ctx);

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

void ScenarioPasteElementsAfter::redo(const score::DocumentContext& ctx) const
{

  Scenario::ProcessModel& scenario = m_ts.find(ctx);

  std::vector<TimeSyncModel*> addedTimeSyncs;
  addedTimeSyncs.reserve(m_json_timesyncs.size());
  std::vector<EventModel*> addedEvents;
  addedEvents.reserve(m_json_events.size());
  for (const auto& timesync : m_json_timesyncs)
  {
    auto tn = new TimeSyncModel(JSONObject::Deserializer{timesync}, &scenario);
    scenario.timeSyncs.add(tn);
    addedTimeSyncs.push_back(tn);
  }

  for (const auto& event : m_json_events)
  {
    auto ev = new EventModel(JSONObject::Deserializer{event}, &scenario);
    scenario.events.add(ev);
    addedEvents.push_back(ev);
  }

  auto& rootSync = scenario.timeSync(m_attachSync);
  for(auto& to_attach : m_eventsToAttach)
  {
    rootSync.addEvent(to_attach);
  }


  auto& stack = score::IDocument::documentContext(scenario).commandStack;
  for (const auto& state : m_json_states)
  {
    scenario.states.add(
        new StateModel(JSONObject::Deserializer{state}, stack, &scenario));
  }

  for (const auto& interval : m_json_intervals)
  {
    auto cst
        = new IntervalModel(JSONObject::Deserializer{interval}, &scenario);
    scenario.intervals.add(cst);
  }

  for (const auto& event : addedEvents)
  {
    updateEventExtent(event->id(), scenario);
  }
  for (const auto& timesync : addedTimeSyncs)
  {
    updateTimeSyncExtent(timesync->id(), scenario);
  }
}

void ScenarioPasteElementsAfter::serializeImpl(DataStreamInput& s) const
{
  s << m_ts << m_attachSync << m_eventsToAttach
    << m_ids_timesyncs << m_ids_events << m_ids_states << m_ids_intervals
    << m_json_timesyncs << m_json_events << m_json_states << m_json_intervals ;
}

void ScenarioPasteElementsAfter::deserializeImpl(DataStreamOutput& s)
{
  s >> m_ts >> m_attachSync >> m_eventsToAttach
      >> m_ids_timesyncs >> m_ids_events >> m_ids_states >> m_ids_intervals
      >> m_json_timesyncs >> m_json_events >> m_json_states
      >> m_json_intervals;
}


}
}
