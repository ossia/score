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
  // For each removed event, we also add its states to the selection.
  // For each removed state, we add the intervals.

  // First we remove :
  // The event if a State is selected and alone

  // Then we have to make a round to remove all the events
  // of the selected time syncs.

  Selection cp = sel;

  bool do_nothing = true;

  sel.clear();
  // if selection only contains TimeSync, remove Trigger or do nothing
  for (const auto& obj : cp)
  {
    if ( auto event = dynamic_cast<const EventModel*>(obj.data()) )
    {
      auto ts = scenar.findTimeSync(event->timeSync());
      if (ts->active())
        ts->setActive(false);
    }
    else if ( !dynamic_cast<const TimeSyncModel*>(obj.data()) )
    {
      sel.append(obj);
      do_nothing = false;
    }
  }

  if (do_nothing)
    return;

  cp = sel;
  for (const auto& obj : cp)
  {
    if (auto state = dynamic_cast<const StateModel*>(obj.data()))
    {
      auto& ev = scenar.events.at(state->eventId());
      if (ev.states().size() == 1)
      {
        sel.append(&ev);
      }
    }
  }

  QList<TimeSyncModel*> maybeRemovedTimenodes;

  cp = sel;
  for (const auto& obj : cp) // Make a copy
  {
    if (auto event = dynamic_cast<const EventModel*>(obj.data()))
    {
      // TODO have scenario take something that takes a container of ids
      // and return the corresponding elements.
      for (const auto& state : event->states())
      {
        sel.append(&scenar.states.at(state));
      }

      // This timesync may be removed if the event is alone.
      auto tn = &scenar.timeSyncs.at(event->timeSync());
      if (!sel.contains(tn))
        maybeRemovedTimenodes.append(tn);
    }
  }

  cp = sel;
  for (const auto& obj : cp)
  {
    if (auto state = dynamic_cast<const StateModel*>(obj.data()))
    {
      if (state->previousInterval())
        sel.append(&scenar.intervals.at(*state->previousInterval()));
      if (state->nextInterval())
        sel.append(&scenar.intervals.at(*state->nextInterval()));
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

    if (auto event = dynamic_cast<const EventModel*>(obj))
    {
      if (event->id() != Id<EventModel>{0})
      {
        QByteArray arr;
        DataStream::Serializer s{&arr};
        s.readFrom(*event);
        m_removedEvents.push_back({event->id(), arr});
      }
    }

    if (auto tn = dynamic_cast<const TimeSyncModel*>(obj))
    {
      if (tn->id() != Id<TimeSyncModel>{0})
      {
        QByteArray arr;
        DataStream::Serializer s2{&arr};
        s2.readFrom(*tn);
        m_removedTimeSyncs.push_back({tn->id(), arr});
      }
    }

    if (auto cmt = dynamic_cast<const CommentBlockModel*>(obj))
    {
      QByteArray arr;
      DataStream::Serializer s{&arr};
      s.readFrom(*cmt);
      m_removedComments.push_back({cmt->id(), arr});
    }

    if (auto interval = dynamic_cast<const IntervalModel*>(obj))
    {
      QByteArray arr;
      DataStream::Serializer s{&arr};
      s.readFrom(*interval);
      m_removedIntervals.push_back({interval->id(), arr});
    }
  }

  // Plus the timesyncs that we don't know if they will be removed (todo ugly
  // fixme pls)
  // TODO how does this even work ? what happens of the maybe removed events /
  // states ?
  for (const auto& tn : maybeRemovedTimenodes)
  {
    if (tn->id() != Id<TimeSyncModel>{0})
    {
      QByteArray arr;
      DataStream::Serializer s2{&arr};
      s2.readFrom(*tn);
      m_maybeRemovedTimeSyncs.push_back({tn->id(), arr});
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

  QList<TimeSyncModel*> maybeTimenodes;
  std::transform(
      m_maybeRemovedTimeSyncs.begin(),
      m_maybeRemovedTimeSyncs.end(),
      std::back_inserter(maybeTimenodes),
      [&](const auto& tndata) {
        DataStream::Deserializer s{tndata.second};
        return new TimeSyncModel{s, &scenar};
      });

  // Recreate all the removed timesyncs
  for (auto& tn : timesyncs)
  {
    // The events should be removed first because else
    // signals may sent and the event may not be found...
    // They will be re-added anyway.
    auto events_in_timesync = tn->events();
    for (auto& event : events_in_timesync)
    {
      tn->removeEvent(event);
    }
    scenar.timeSyncs.add(tn);
  }

  // Recreate first all the events / maybe removed timesyncs
  for (auto& event : events)
  {
    // We have to make a copy at each iteration since each iteration
    // might add a timesync.
    auto timesyncs_in_scenar = shallow_copy(scenar.timeSyncs.map());
    auto scenar_timesync_it = std::find(
        timesyncs_in_scenar.begin(),
        timesyncs_in_scenar.end(),
        event->timeSync());
    if (scenar_timesync_it != timesyncs_in_scenar.end())
    {
      // The timesync already exists
      // Hence we don't need the one we serialized.
      auto to_delete = std::find(
          maybeTimenodes.begin(), maybeTimenodes.end(), event->timeSync());

      // TODO why do we need to check for this ? SCORE_ASSERT sometime
      // fails...
      if (to_delete != maybeTimenodes.end())
      {
        delete *to_delete;
        maybeTimenodes.erase(to_delete);
      }

      // We can add our event to the scenario.
      scenar.events.add(event);

      // Maybe this shall be done after everything has been added to prevent
      // problems ?
      (*scenar_timesync_it)->addEvent(event->id());
    }
    else
    {
      // We have to insert the timesync that was removed.
      auto removed_timesync_it = std::find(
          maybeTimenodes.begin(), maybeTimenodes.end(), event->timeSync());
      SCORE_ASSERT(removed_timesync_it != maybeTimenodes.end());
      TimeSyncModel* timeSync = *removed_timesync_it;

      maybeTimenodes.erase(removed_timesync_it);

      // First, since the event is not yet in the scenario
      // we remove it from the timesync since it might crash
      timeSync->removeEvent(event->id());

      // And we add the timesync
      scenar.timeSyncs.add(timeSync);

      // We can re-add the event.
      scenar.events.add(event);
      timeSync->addEvent(event->id());
    }
  }

  // All the states
  for (const auto& state : states)
  {
    scenar.states.add(state);
    scenar.event(state->eventId()).addState(state->id());
  }

  for (const auto& cmt : comments)
  {
    scenar.comments.add(cmt);
  }

  // And then all the intervals.
  for (const auto& intervaldata : m_removedIntervals)
  {
    DataStream::Deserializer s{intervaldata.second};
    auto cstr = new IntervalModel{s, &scenar};

    scenar.intervals.add(cstr);

    // Set-up the start / end events correctly.
    SetNextInterval(startState(*cstr, scenar), *cstr);
    SetPreviousInterval(endState(*cstr, scenar), *cstr);
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

  // Remove the intervals
  for (const auto& cstr : m_removedIntervals)
  {
    StandardRemovalPolicy::removeInterval(scenar, cstr.first);
  }

  // The other things
  for (const auto& ev : m_removedEvents)
  {
    StandardRemovalPolicy::removeEventStatesAndIntervals(scenar, ev.first);
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
  s << m_path << m_maybeRemovedTimeSyncs << m_removedEvents
    << m_removedTimeSyncs << m_removedIntervals << m_removedStates
    << m_removedComments;
}

void RemoveSelection::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_maybeRemovedTimeSyncs >> m_removedEvents
      >> m_removedTimeSyncs >> m_removedIntervals >> m_removedStates
      >> m_removedComments;
}
}
}
