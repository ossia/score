// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "RemoveSelection.hpp"

#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <Scenario/Process/Algorithms/StandardRemovalPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/IdentifiedObjectAbstract.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/MapCopy.hpp>

#include <QList>
#include <QSet>

namespace Scenario
{
namespace Command
{
RemoveSelection::RemoveSelection(const Scenario::ProcessModel& scenar, Selection sel)
    : m_path{scenar}
{
  // Serialize all the events and intervals and timesyncs and states and
  // comments

  // For each removed interval, we also add its states to the selection if they
  // are empty

  // We add Event to the element to delete if all its States are selected
  // Wa add TimeSync to the list if all its node are selected for deletion

  // Then we have to make a round to remove all the events
  // of the selected time syncs.

  Selection cp = sel;

  // First add start/end state of soon-to-be-removed interval
  // but only if they are empty with no other interval
  for (const auto& obj : cp)
  {
    if (auto interval = dynamic_cast<const IntervalModel*>(obj.data()))
    {
      auto start_id = interval->startState();
      auto& start = scenar.states.at(start_id);
      if (start.empty() && !start.previousInterval())
        sel.append(start);

      auto end_id = interval->endState();
      auto& end = scenar.states.at(end_id);
      if (end.empty() && !end.nextInterval())
        sel.append(end);
    }
  }

  // Then add Event to selection if we select all its States

  cp = sel;
  for (const auto& obj : cp)
  {
    if (auto state = dynamic_cast<const StateModel*>(obj.data()))
    {
      auto& ev = scenar.events.at(state->eventId());
      bool add_event = true;
      for (const auto& child : ev.states())
      {
        auto& st = scenar.states.at(child);
        if (!sel.contains(&st))
        {
          add_event = false;
          break;
        }
      }
      if (add_event)
      {
        sel.append(ev);
      }
    }
  }

  cp = sel;
  for (const auto& obj : cp)
  {
    if (auto event = dynamic_cast<const EventModel*>(obj.data()))
    {
      Scenario::TimeSyncModel& ts = scenar.timeSyncs.at(event->timeSync());
      bool add_ts = true;
      for (const auto& child : ts.events())
      {
        auto& st = scenar.events.at(child);
        if (!sel.contains(&st))
        {
          add_ts = false;
          break;
        }
      }
      if (add_ts)
      {
        sel.append(ts);
      }
    }
  }

  sel.removeDuplicates();

  QObjectList l;
  l.reserve(sel.size());
  for (const QPointer<IdentifiedObjectAbstract>& p : sel)
    l.push_back(p.data());
  m_cables = Dataflow::saveCables(l, score::IDocument::documentContext(scenar));

  // Serialize ALL the things
  for (const QPointer<IdentifiedObjectAbstract>& ptr : sel)
  {
    auto obj = ptr.data();
    if (auto state = dynamic_cast<const StateModel*>(obj))
    {
      if (Q_UNLIKELY(state->id() != Scenario::startId<StateModel>()))
      {
        QByteArray arr;
        DataStream::Serializer s{&arr};
        s.readFrom(*state);
        m_removedStates.push_back({state->id(), arr});
      }
    }

    else if (auto event = dynamic_cast<const EventModel*>(obj))
    {
      if (Q_LIKELY(event->id() != Scenario::startId<EventModel>()))
      {
        QByteArray arr;
        DataStream::Serializer s{&arr};
        s.readFrom(*event);
        m_cleanedEvents.push_back({event->id(), arr});
      }
    }

    else if (auto ts = dynamic_cast<const TimeSyncModel*>(obj))
    {
      if (Q_LIKELY(ts->id() != Scenario::startId<TimeSyncModel>()))
      {
        QByteArray arr;
        DataStream::Serializer s2{&arr};
        s2.readFrom(*ts);
        m_cleanedTimeSyncs.push_back({ts->id(), arr});
        for (const auto& cstrId : intervalsBeforeTimeSync(scenar, ts->id()))
        {
          m_cmds_set_rigidity.emplace_back(scenar.interval(cstrId), true);
        }
      }
    }

    else if (auto cmt = dynamic_cast<const CommentBlockModel*>(obj))
    {
      QByteArray arr;
      DataStream::Serializer s{&arr};
      s.readFrom(*cmt);
      m_removedComments.push_back({cmt->id(), arr});
    }

    else if (auto interval = dynamic_cast<const IntervalModel*>(obj))
    {
      QByteArray arr;
      DataStream::Serializer s{&arr};
      s.readFrom(*interval);
      m_removedIntervals.push_back({interval->id(), arr});
    }
  }
}

void RemoveSelection::undo(const score::DocumentContext& ctx) const
{
  auto& scenar = m_path.find(ctx);
  // First instantiate everything

  QList<StateModel*> states;
  std::transform(
      m_removedStates.begin(),
      m_removedStates.end(),
      std::back_inserter(states),
      [&](const auto& data) {
        DataStream::Deserializer s{data.second};
        return new StateModel{s, scenar.context(), &scenar};
      });

  QList<EventModel*> events;
  std::transform(
      m_cleanedEvents.begin(),
      m_cleanedEvents.end(),
      std::back_inserter(events),
      [&](const auto& eventdata) {
        DataStream::Deserializer s{eventdata.second};
        return new EventModel{s, &scenar};
      });

  QList<TimeSyncModel*> timesyncs;
  std::transform(
      m_cleanedTimeSyncs.begin(),
      m_cleanedTimeSyncs.end(),
      std::back_inserter(timesyncs),
      [&](const auto& tndata) {
        DataStream::Deserializer s{tndata.second};
        return new TimeSyncModel{s, &scenar};
      });

  QList<CommentBlockModel*> comments;
  std::transform(
      m_removedComments.begin(),
      m_removedComments.end(),
      std::back_inserter(comments),
      [&](const auto& cmtdata) {
        DataStream::Deserializer s{cmtdata.second};
        return new CommentBlockModel(s, &scenar);
      });

  // Recreate all the removed timesyncs or only restore their trigger
  for (auto& timesync : timesyncs)
  {
    auto ts = scenar.findTimeSync(timesync->id());

    if (ts)
    {
      if (timesync->active())
        ts->setActive(true);
      else
        ts->setActive(false);
      delete timesync;
    }
    else
    {
      // The events should be removed first because else
      // signals may sent and the event may not be found...
      // They will be re-added anyway.
      auto events_in_timesync = timesync->events();
      for (auto& event : events_in_timesync)
      {
        timesync->removeEvent(event);
      }
      scenar.timeSyncs.add(timesync);
    }
  }

  // Recreate all the events / maybe removed timesyncs
  for (auto& event : events)
  {

    // first check if the node is already present
    auto ev = scenar.findEvent(event->id());

    if (ev)
    {
      // if so, reset it's condition
      ev->setCondition(event->condition());
      delete event;
    }
    else
    {

      // We have to make a copy at each iteration since each iteration
      // might add a timesync.
      auto timesyncs_in_scenar = shallow_copy(scenar.timeSyncs.map());
      auto scenar_timesync_it
          = std::find(timesyncs_in_scenar.begin(), timesyncs_in_scenar.end(), event->timeSync());
      if (scenar_timesync_it != timesyncs_in_scenar.end())
      {
        // We can add our event to the scenario.
        scenar.events.add(event);

        // Maybe this shall be done after everything has been added to prevent
        // problems ?
        (*scenar_timesync_it)->addEvent(event->id());
      }
    }
  }

  // All the states
  for (const auto& state : states)
  {
    auto st = scenar.findState(state->id());
    if (!st)
    {
      scenar.states.add(state);
      scenar.event(state->eventId()).addState(state->id());
    }
    else
    {
      delete state;
    }
  }

  for (const auto& cmt : comments)
  {
    scenar.comments.add(cmt);
  }

  // And then all the intervals.
  for (const auto& intervaldata : m_removedIntervals)
  {
    DataStream::Deserializer s{intervaldata.second};
    auto itv = new IntervalModel{s, scenar.context(), &scenar};

    scenar.intervals.add(itv);

    // Set-up the start / end events correctly.
    auto& ss = startState(*itv, scenar);
    auto& es = endState(*itv, scenar);
    SetNextInterval(ss, *itv);
    SetPreviousInterval(es, *itv);

    auto& sev = parentEvent(ss, scenar);
    auto& eev = parentEvent(es, scenar);
  }

  for (const auto& cmd : m_cmds_set_rigidity)
  {
    cmd.undo(ctx);
  }

  // This will also recompute timesync extents
  Dataflow::restoreCables(m_cables, ctx);
}

void RemoveSelection::redo(const score::DocumentContext& ctx) const
{
  Dataflow::removeCables(m_cables, ctx);
  auto& scenar = m_path.find(ctx);

  for (const auto& cmd : m_cmds_set_rigidity)
  {
    cmd.redo(ctx);
  }

  // Remove the intervals
  for (const auto& itv : m_removedIntervals)
  {
    StandardRemovalPolicy::removeInterval(scenar, itv.first);
  }

  // The other things

  for (const auto& cmt : m_removedComments)
  {
    auto it = scenar.comments.find(cmt.first);
    if (it != scenar.comments.end())
      StandardRemovalPolicy::removeComment(scenar, *it);
  }

  // Finally if there are remaining states
  for (const auto& st : m_removedStates)
  {
    auto it = scenar.states.find(st.first);
    if (it != scenar.states.end())
    {
      StandardRemovalPolicy::removeState(scenar, *it);
    }
  }

  for (const auto& ev : m_cleanedEvents)
  {
    auto e = scenar.findEvent(ev.first);

    if (e->states().size() == 0)
    {
      scenar.timeSyncs.at(e->timeSync()).removeEvent(e->id());
      scenar.events.remove(e);
    }
    else
    {
      // remove condition on selected Events
      if (e->condition() != State::Expression{})
      {
        e->setCondition({});
      }
    }
  }

  for (const auto& tsid : m_cleanedTimeSyncs)
  {
    auto ts = scenar.findTimeSync(tsid.first);
    if (ts->events().size() == 0)
    {
      scenar.timeSyncs.remove(ts);
    }
    else
    {
      ts->setActive(false);
    }
  }
}

void RemoveSelection::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_cleanedEvents << m_cleanedTimeSyncs << m_removedIntervals << m_removedStates
    << m_removedComments;

  s << (int32_t)m_cmds_set_rigidity.size();
  for (const auto& cmd : m_cmds_set_rigidity)
  {
    s << cmd.serialize();
  }

  s << m_cables;
}

void RemoveSelection::deserializeImpl(DataStreamOutput& s)
{
  int32_t n;
  s >> m_path >> m_cleanedEvents >> m_cleanedTimeSyncs >> m_removedIntervals >> m_removedStates
      >> m_removedComments >> n;

  m_cmds_set_rigidity.resize(n);
  for (int i = 0; i < n; i++)
  {
    QByteArray a;
    s >> a;
    m_cmds_set_rigidity[i].deserialize(a);
  }

  s >> m_cables;
}
}
}
