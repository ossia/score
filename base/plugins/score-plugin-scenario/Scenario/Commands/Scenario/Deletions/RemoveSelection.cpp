// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Process/Algorithms/StandardRemovalPolicy.hpp>

#include <QDataStream>
#include <QList>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QtGlobal>
#include <algorithm>
#include <boost/iterator/indirect_iterator.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iterator>

#include "RemoveSelection.hpp"
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/IdentifiedObjectAbstract.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/tools/MapCopy.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/Identifier.hpp>

namespace Scenario
{
namespace Command
{
RemoveSelection::RemoveSelection(
    const Scenario::ProcessModel& scenar, Selection sel)
    : m_path{scenar}
{
  // Serialize all the events and intervals and timesyncs and states and
  // comments

  // For each removed interval, we also add its states to the selection if they are empty

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
        sel.append(&start);

      auto end_id = interval->endState();
      auto& end = scenar.states.at(end_id);
      if (end.empty() && !end.nextInterval())
        sel.append(&end);
    }
  }

  // If we select a TimeSync for deletion, put its events into new separate TimeSync
  cp = sel;
  for ( const auto& obj : cp)
  {
    if (auto ts = dynamic_cast<const TimeSyncModel*>(obj.data()))
    {
      if(ts->events().size()>1)
      {
        bool split = false;
        for (auto child : ts->events()){
          auto& ev = scenar.events.at(child);
          if (!sel.contains(&ev)){
            split = true;
            break;
          }
        }

        if (split){
          for (int i = 1; i < ts->events().size(); i++)
          {
            QVector<Id<EventModel>> move_me{ts->events()[i]};
            m_cmds_split_timesync.emplace_back(*ts, move_me);
          }

          // if we split the timesync, then we won't remove it
          sel.removeAll(obj);
        }
      }
    }
  }

  // If we select an Event for deletion, put its states into new separate events
  for ( const auto& obj : cp)
  {
    if (auto ev = dynamic_cast<const EventModel*>(obj.data()))
    {
      if (ev->states().size()>1)
      {
        bool split =  false;
        for (auto child : ev->states()){
          auto& st = scenar.states.at(child);
          if (!sel.contains(&st)){
            split = true;
            break;
          }
        }
        if (split){
          for (int i = 1; i < ev->states().size(); i++)
          {
            QVector<Id<StateModel>> move_me{ev->states()[i]};
            m_cmds_split_event.emplace_back(scenar, ev->id(), move_me);
          }

          // if we split the event, then we won't remove it
          sel.removeAll(obj);
        }
      }
    }
  }

  // Then add Event to selection if we select all its States
  for (const auto& obj : cp)
  {
    if (auto state = dynamic_cast<const StateModel*>(obj.data()))
    {
      auto& ev = scenar.events.at(state->eventId());
      bool add_event=true;
      for (auto child : ev.states()){
        auto& st = scenar.states.at(child);
        if (!sel.contains(&st)){
          add_event = false;
          break;
        }
      }
      if (add_event)
      {
        sel.append(&ev);
      }
    }
  }

  auto purged = sel.toList().toSet().toList();

  // Serialize ALL the things
  for (const auto& obj : purged)
  {
    if (auto state = dynamic_cast<const StateModel*>(obj))
    {
      QByteArray arr;
      DataStream::Serializer s{&arr};
      s.readFrom(*state);
      m_removedStates.push_back({state->id(), arr});
    }

    else if (auto event = dynamic_cast<const EventModel*>(obj))
    {
      if (event->id() != Id<EventModel>{0})
      {
        QByteArray arr;
        DataStream::Serializer s{&arr};
        s.readFrom(*event);
        m_removedEvents.push_back({event->id(), arr});
      }
    }

    else if (auto ts = dynamic_cast<const TimeSyncModel*>(obj))
    {
      if (ts->id() != Id<TimeSyncModel>{0})
      {
        QByteArray arr;
        DataStream::Serializer s2{&arr};
        s2.readFrom(*ts);
        m_removedTimeSyncs.push_back({ts->id(), arr});
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

  auto& stack = score::IDocument::documentContext(scenar).commandStack;
  // First instantiate everything

  QList<StateModel*> states;
  std::transform(
      m_removedStates.begin(),
      m_removedStates.end(),
      std::back_inserter(states),
      [&](const auto& data) {
        DataStream::Deserializer s{data.second};
        return new StateModel{s, stack, &scenar};
      });

  QList<EventModel*> events;
  std::transform(
      m_removedEvents.begin(),
      m_removedEvents.end(),
      std::back_inserter(events),
      [&](const auto& eventdata) {
        DataStream::Deserializer s{eventdata.second};
        return new EventModel{s, &scenar};
      });

  QList<TimeSyncModel*> timesyncs;
  std::transform(
      m_removedTimeSyncs.begin(),
      m_removedTimeSyncs.end(),
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

    if (ts){
      if (timesync->active())
        ts->setActive(true);
      else
        ts->setActive(false);
    } else {
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
    } else {

      // We have to make a copy at each iteration since each iteration
      // might add a timesync.
      auto timesyncs_in_scenar = shallow_copy(scenar.timeSyncs.map());
      auto scenar_timesync_it = std::find(
                                  timesyncs_in_scenar.begin(),
                                  timesyncs_in_scenar.end(),
                                  event->timeSync());
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
    if (!st){
      scenar.states.add(state);
      scenar.event(state->eventId()).addState(state->id());
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
    auto itv = new IntervalModel{s, &scenar};

    scenar.intervals.add(itv);

    // Set-up the start / end events correctly.
    SetNextInterval(startState(*itv, scenar), *itv);
    SetPreviousInterval(endState(*itv, scenar), *itv);
  }

  for (const auto& cmd : m_cmds_set_rigidity)
  {
    cmd.undo(ctx);
  }

  // Split Event and TimeSync
  for (const auto& cmd : m_cmds_split_event)
  {
    cmd.undo(ctx);
  }
  for (const auto& cmd : m_cmds_split_timesync)
  {
    cmd.undo(ctx);
  }

  for (auto& tn : scenar.timeSyncs)
  {
    updateTimeSyncExtent(tn.id(), scenar);
  }
  for (auto& ev : scenar.events)
  {
    updateEventExtent(ev.id(), scenar);
  }
}

void RemoveSelection::redo(const score::DocumentContext& ctx) const
{
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

  // Split Event and TimeSync
  for (const auto& cmd : m_cmds_split_event)
  {
    cmd.redo(ctx);
  }
  for (const auto& cmd : m_cmds_split_timesync)
  {
    cmd.redo(ctx);
  }

  // The other things

  // remove condition on selected Events
  for (const auto& ev : m_removedEvents)
  {
    auto e = scenar.findEvent(ev.first);
    if (e->condition() != State::Expression{})
    {
      e->setCondition({});
    }
  }

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

  for (const auto& tsid : m_removedTimeSyncs)
  {
    auto ts = scenar.findTimeSync(tsid.first);
    if (ts->active())
    {
      ts->setActive(false);
    }
  }

  for (auto& tn : scenar.timeSyncs)
  {
    updateTimeSyncExtent(tn.id(), scenar);
  }
  for (auto& ev : scenar.events)
  {
    updateEventExtent(ev.id(), scenar);
  }
}

void RemoveSelection::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_removedEvents
    << m_removedTimeSyncs << m_removedIntervals << m_removedStates
    << m_removedComments;

  s << (int32_t)m_cmds_set_rigidity.size();
  for (const auto& cmd : m_cmds_set_rigidity)
  {
    s << cmd.serialize();
  }

  s << (int32_t)m_cmds_split_event.size();
  for (const auto& cmd : m_cmds_split_event)
  {
    s << cmd.serialize();
  }
  s << (int32_t)m_cmds_split_timesync.size();
  for (const auto& cmd : m_cmds_split_timesync)
  {
    s << cmd.serialize();
  }
}

void RemoveSelection::deserializeImpl(DataStreamOutput& s)
{
  int32_t n;
  s >> m_path >> m_removedEvents
      >> m_removedTimeSyncs >> m_removedIntervals >> m_removedStates
      >> m_removedComments >> n;

  m_cmds_set_rigidity.resize(n);
  for (int i = 0; i < n; i++)
  {
    QByteArray a;
    s >> a;
    m_cmds_set_rigidity[i].deserialize(a);
  }

  s >> n;
  m_cmds_split_event.resize(n);
  for (int i = 0; i < n; i++)
  {
    QByteArray a;
    s >> a;
    m_cmds_split_event[i].deserialize(a);
  }

  s >> n;
  m_cmds_split_timesync.resize(n);
  for (int i = 0; i < n; i++)
  {
    QByteArray a;
    s >> a;
    m_cmds_split_timesync[i].deserialize(a);
  }
}
}
}
