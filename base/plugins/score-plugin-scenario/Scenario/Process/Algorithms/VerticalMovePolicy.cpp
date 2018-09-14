// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "VerticalMovePolicy.hpp"

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/model/EntityMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/detail/flat_set.hpp>

#include <limits>

namespace Scenario
{
void updateEventExtent(const Id<EventModel>& id, Scenario::ProcessModel& s)
{
  auto& ev = s.event(id);
  double min = std::numeric_limits<double>::max();
  double max = std::numeric_limits<double>::lowest();
  for (const auto& state_id : ev.states())
  {
    const auto& st = s.states.at(state_id);

    if (st.heightPercentage() < min)
      min = st.heightPercentage();
    if (st.heightPercentage() > max)
      max = st.heightPercentage();
  }

  ev.setExtent({min, max});
  updateTimeSyncExtent(ev.timeSync(), s);
}

void updateTimeSyncExtent(
    const Id<TimeSyncModel>& id, Scenario::ProcessModel& s)
{
  auto& tn = s.timeSyncs.at(id);
  double min = std::numeric_limits<double>::max();
  double max = std::numeric_limits<double>::lowest();
  for (const auto& ev_id : tn.events())
  {
    const auto& ev = s.events.at(ev_id);
    if (ev.extent().top() < min)
      min = ev.extent().top();
    if (ev.extent().bottom() > max)
      max = ev.extent().bottom();
  }

  tn.setExtent({min, max});
}

void updateIntervalVerticalPos(
    double y, const Id<IntervalModel>& id, Scenario::ProcessModel& s)
{
  auto& cst = s.intervals.at(id);

  // First make the list of all the intervals to update
  static ossia::flat_set<IntervalModel*> intervalsToUpdate;
  static ossia::flat_set<StateModel*> statesToUpdate;

  intervalsToUpdate.insert(&cst);
  StateModel* rec_state = &s.state(cst.startState());

  statesToUpdate.insert(rec_state);
  while (rec_state->previousInterval())
  {
    IntervalModel* rec_cst = &s.intervals.at(*rec_state->previousInterval());
    intervalsToUpdate.insert(rec_cst);
    statesToUpdate.insert(rec_state);
    rec_state = &s.states.at(rec_cst->startState());
  }
  statesToUpdate.insert(rec_state); // Add the first state

  rec_state = &s.state(cst.endState());
  statesToUpdate.insert(rec_state);
  while (rec_state->nextInterval())
  {
    IntervalModel* rec_cst = &s.intervals.at(*rec_state->nextInterval());
    intervalsToUpdate.insert(rec_cst);
    statesToUpdate.insert(rec_state);
    rec_state = &s.states.at(rec_cst->endState());
  }
  statesToUpdate.insert(rec_state); // Add the last state

  // Set the correct height
  for (auto& interval : intervalsToUpdate)
  {
    interval->setHeightPercentage(y);
    s.intervalMoved(*interval);
  }

  for (auto& state : statesToUpdate)
  {
    state->setHeightPercentage(y);
    updateEventExtent(state->eventId(), s);
  }

  intervalsToUpdate.clear();
  statesToUpdate.clear();
}
}
