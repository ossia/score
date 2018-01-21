// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "StandardRemovalPolicy.hpp"
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <QDebug>
#include <QVector>
#include <boost/iterator/indirect_iterator.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <score/tools/std/Optional.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <score/model/EntityMap.hpp>
#include <score/tools/MapCopy.hpp>
#include <score/model/Identifier.hpp>

namespace Scenario
{
static void removeEventFromTimeSync(
    Scenario::ProcessModel& scenario, const Id<EventModel>& eventId)
{
  // We have to make a copy else the iterator explodes.
  auto timesyncs = shallow_copy(scenario.timeSyncs.map());
  for (auto timeSync : timesyncs)
  {
    if (timeSync->removeEvent(eventId))
    {
      scenario.events.remove(eventId);
      if (timeSync->events().empty())
      {
        // TODO transform this into a class with algorithms on timesyncs +
        // scenario, etc.
        // Note : this changes the scenario.timeSyncs() iterator, however
        // since we return afterwards there is no problem.
        ScenarioCreate<TimeSyncModel>::undo(timeSync->id(), scenario);
      }
    }
  }
}

void StandardRemovalPolicy::removeInterval(
    Scenario::ProcessModel& scenario, const Id<IntervalModel>& intervalId)
{
  auto cstr_it = scenario.intervals.find(intervalId);
  if (cstr_it != scenario.intervals.end())
  {
    IntervalModel& cstr = *cstr_it;

    SetNoNextInterval(startState(cstr, scenario));
    SetNoPreviousInterval(endState(cstr, scenario));

    scenario.intervals.remove(&cstr);
  }
  else
  {
    qDebug() << Q_FUNC_INFO << "Warning : removing a non-existant interval";
  }
}

void StandardRemovalPolicy::removeState(
    Scenario::ProcessModel& scenario, StateModel& state)
{
  if (!state.previousInterval() && !state.nextInterval())
  {
    auto& ev = scenario.events.at(state.eventId());
    ev.removeState(state.id());

    scenario.states.remove(&state);

    updateEventExtent(ev.id(), scenario);
  }
}

void StandardRemovalPolicy::removeEventStatesAndIntervals(
    Scenario::ProcessModel& scenario, const Id<EventModel>& eventId)
{
  auto& ev = scenario.event(eventId);

  auto states = ev.states();
  for (const auto& state : states)
  {
    auto it = scenario.states.find(state);
    if (it != scenario.states.end())
      StandardRemovalPolicy::removeState(scenario, *it);
  }

  removeEventFromTimeSync(scenario, eventId);
}

void StandardRemovalPolicy::removeComment(
    Scenario::ProcessModel& scenario, CommentBlockModel& cmt)
{
  scenario.comments.remove(&cmt);
}
}
