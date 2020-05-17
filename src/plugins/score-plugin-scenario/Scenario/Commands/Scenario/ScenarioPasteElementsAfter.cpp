// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioPasteElementsAfter.hpp"

#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Scenario/ScenarioPaste.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/model/path/ObjectPath.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateModel.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/Clamp.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/detail/algorithms.hpp>

#include <cstddef>
#include <limits>
#include <vector>

#include <unordered_map>
namespace Scenario
{
namespace Command
{
ScenarioPasteElementsAfter::ScenarioPasteElementsAfter(
    const Scenario::ProcessModel& scenario,
    const Scenario::TimeSyncModel& attach_sync,
    const rapidjson::Value& obj,
    double ratio)
    : m_ts{scenario}
{
  m_attachSync = attach_sync.id();

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

  SCORE_ASSERT(!timesyncs.empty());

  // We set the new ids everywhere
  {
    // Find the earliest timesync (will serve of attach point)
    auto earliest_it = timesyncs.begin();
    auto min_date = (*earliest_it)->date();
    for (auto it = earliest_it; it != timesyncs.end(); ++it)
    {
      if ((*it)->date() < min_date)
      {
        min_date = (*it)->date();
        earliest_it = it;
      }
    }

    // Remove it and set its events to the relevant sync in the parent scenario
    std::vector<EventModel*> event_in_start;
    {
      auto timesync = *earliest_it;
      for (EventModel* event : events)
      {
        if (event->timeSync() == timesync->id())
        {
          event->changeTimeSync(m_attachSync);
          event_in_start.push_back(event);
        }
      }
      delete timesync;
      timesyncs.erase(earliest_it);
    }

    // Set new ids for all the timesyncs
    int i = 0;
    for (auto it = timesyncs.begin(); it != timesyncs.end(); ++i, ++it)
    {
      auto timesync = *it;
      for (EventModel* event : events)
      {
        if (event->timeSync() == timesync->id() && !ossia::contains(event_in_start, event))
        {
          event->changeTimeSync(timesync_ids[i]);
        }
      }
      timesync->setId(timesync_ids[i]);
    }
  }

  {
    int i = 0;
    for (EventModel* event : events)
    {
      {
        auto it = std::find_if(timesyncs.begin(), timesyncs.end(), [&](TimeSyncModel* tn) {
          return tn->id() == event->timeSync();
        });
        if (it != timesyncs.end())
        {
          auto timesync = *it;
          timesync->removeEvent(event->id());
          timesync->addEvent(event_ids[i]);
        }
        else
        {
          SCORE_ASSERT(event->timeSync() == m_attachSync);
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
        auto it = std::find_if(events.begin(), events.end(), [&](EventModel* event) {
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
  {
    std::unordered_map<Id<IntervalModel>, Id<IntervalModel>> id_map;
    {
      int i = 0;
      for (IntervalModel* interval : intervals)
      {
        id_map[interval->id()] = interval_ids[i];
        i++;
      }
    }

    auto& doc = score::IDocument::modelDelegate<ScenarioDocumentModel>(ctx.document);
    auto cable_ids = getStrongIdRange<Process::Cable>(cables.size(), doc.cables);

    int i = 0;
    Path<Process::ProcessModel> p{scenario};
    for (Process::CableData& cd : cables)
    {
      auto& source_vec = cd.source.unsafePath().vec();
      auto& sink_vec = cd.sink.unsafePath().vec();
      int32_t source_itv_id = source_vec.front().id();
      int32_t sink_itv_id = sink_vec.front().id();

      for (IntervalModel* interval : intervals)
      {
        auto id = interval->id().val();
        if (id == source_itv_id)
          source_itv_id = id_map.at(interval->id()).val();
        if (id == sink_itv_id)
          sink_itv_id = id_map.at(interval->id()).val();
      }
      source_vec.front() = ObjectIdentifier{source_vec.front().objectName(), source_itv_id};
      sink_vec.front() = ObjectIdentifier{sink_vec.front().objectName(), sink_itv_id};

      source_vec.insert(
          source_vec.begin(), p.unsafePath().vec().begin(), p.unsafePath().vec().end());
      sink_vec.insert(sink_vec.begin(), p.unsafePath().vec().begin(), p.unsafePath().vec().end());

      m_cables.insert(cable_ids[i], std::move(cd));
      i++;
    }
  }

  {
    int i = 0;
    for (IntervalModel* interval : intervals)
    {
      const auto ports = interval->findChildren<Process::Port*>();
      for (Process::Port* port : ports)
      {
        while (!port->cables().empty())
        {
          port->removeCable(port->cables().back());
        }
      }

      interval->setId(interval_ids[i]);
      {
        auto start_state_id = ossia::find_if(
            states, [&](auto state) { return state->id() == interval->startState(); });
        if (start_state_id != states.end())
          SetNextInterval(**start_state_id, *interval);
      }
      {
        auto end_state_id = ossia::find_if(
            states, [&](auto state) { return state->id() == interval->endState(); });
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
    auto earliestTime = !intervals.empty() ? intervals.front()->date() : timesyncs.front()->date();
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

    auto delta_t = attach_sync.date() - earliestTime;
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
  qreal delta_y = 0;
  for (auto& ev : attach_sync.events())
  {
    for (auto& state : scenario.events.at(ev).states())
    {
      auto h = scenario.states.at(state).heightPercentage();
      if (h > delta_y)
        delta_y = h;
    }
  }

  delta_y *= 1.1;
  for (IntervalModel* cst : intervals)
  {
    cst->setHeightPercentage(clamp(delta_y, 0., 1.));
  }
  for (StateModel* state : states)
  {
    state->setHeightPercentage(clamp(delta_y, 0., 1.));
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

void ScenarioPasteElementsAfter::undo(const score::DocumentContext& ctx) const
{
  // TODO remove added events
  auto& scenario = m_ts.find(ctx);

  ScenarioDocumentModel& model
      = score::IDocument::modelDelegate<ScenarioDocumentModel>(ctx.document);
  for (const auto& cable_id : m_cables.keys())
  {
    auto& c = model.cables.at(cable_id);
    c.source().find(ctx).removeCable(c);
    c.sink().find(ctx).removeCable(c);
    model.cables.remove(cable_id);
  }
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

  auto& rootSync = scenario.timeSync(m_attachSync);
  for (auto& to_attach : m_eventsToAttach)
  {
    rootSync.addEvent(to_attach);
  }

  for (const auto& state : m_json_states)
  {
    auto st = new StateModel(DataStream::Deserializer{state}, scenario.context(), &scenario);
    scenario.states.add(st);
  }

  for (const auto& interval : m_json_intervals)
  {
    auto cst
        = new IntervalModel(DataStream::Deserializer{interval}, scenario.context(), &scenario);
    scenario.intervals.add(cst);
  }

  ScenarioDocumentModel& model
      = score::IDocument::modelDelegate<ScenarioDocumentModel>(ctx.document);
  for (const auto& cable_id : m_cables.keys())
  {
    const auto& dat = m_cables[cable_id];
    auto c = new Process::Cable{cable_id, dat, &model};

    Path<Scenario::ScenarioDocumentModel> model_path{model};

    model.cables.add(c);
    auto ext = model_path.extend(cable_id);
    dat.source.find(ctx).addCable(*c);
    dat.sink.find(ctx).addCable(*c);
  }
}

void ScenarioPasteElementsAfter::serializeImpl(DataStreamInput& s) const
{
  s << m_ts << m_attachSync << m_eventsToAttach << m_ids_timesyncs << m_ids_events << m_ids_states
    << m_ids_intervals << m_json_timesyncs << m_json_events << m_json_states << m_json_intervals
    << m_cables;
}

void ScenarioPasteElementsAfter::deserializeImpl(DataStreamOutput& s)
{
  s >> m_ts >> m_attachSync >> m_eventsToAttach >> m_ids_timesyncs >> m_ids_events >> m_ids_states
      >> m_ids_intervals >> m_json_timesyncs >> m_json_events >> m_json_states >> m_json_intervals
      >> m_cables;
}
}
}
