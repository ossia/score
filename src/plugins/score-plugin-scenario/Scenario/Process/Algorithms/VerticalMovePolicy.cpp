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
void updateTimeSyncExtent(
    TimeSyncModel& tn,
    const Scenario::ProcessModel& s)
{
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

  if(max - min > 3.)
  {
    min += 1.;
    max -= 1.;
  }

  tn.setExtent({min, max});
}

void updateEventExtent(EventModel& ev, const Scenario::ProcessModel& s, double view_height)
{
  if(view_height <= 2.)
    return;

  double min = std::numeric_limits<double>::max();
  double max = std::numeric_limits<double>::lowest();

  for (const auto& state_id : ev.states())
  {
    const auto& st = s.states.at(state_id);

    if (st.heightPercentage() < min)
      min = st.heightPercentage();
    if (st.heightPercentage() > max)
      max = st.heightPercentage();

    if (const auto& itv_id = st.previousInterval())
    {
      const IntervalModel& itv = s.intervals.at(*itv_id);
      const double h = (1. + itv.getHeight()) / view_height;
      if(itv.smallViewVisible() && st.heightPercentage() + h > max)
      {
        max = st.heightPercentage() + h;
      }
    }
    if (const auto& itv_id = st.nextInterval())
    {
      const IntervalModel& itv = s.intervals.at(*itv_id);
      const double h = (1. + itv.getHeight()) / view_height;
      if(itv.smallViewVisible() && st.heightPercentage() + h > max)
      {
        max = st.heightPercentage() + h;
      }
    }
  }

  ev.setExtent({min, max});
  // TODO we could maybe skip this in case where the event
  // grows ?
  updateTimeSyncExtent(s.timeSyncs.at(ev.timeSync()), s);
}

void updateIntervalVerticalPos(
    IntervalModel& itv,
    double y,
    const Scenario::ProcessModel& s,
    double view_height)
{
  if(view_height <= 2.)
    return;

  // First make the list of all the intervals to update
  static ossia::flat_set<IntervalModel*> intervalsToUpdate;
  static ossia::flat_set<StateModel*> statesToUpdate;

  intervalsToUpdate.insert(&itv);
  StateModel* rec_state = &s.state(itv.startState());

  statesToUpdate.insert(rec_state);
  while (rec_state->previousInterval())
  {
    IntervalModel* rec_cst = &s.intervals.at(*rec_state->previousInterval());
    intervalsToUpdate.insert(rec_cst);
    statesToUpdate.insert(rec_state);
    rec_state = &s.states.at(rec_cst->startState());
  }
  statesToUpdate.insert(rec_state); // Add the first state

  rec_state = &s.state(itv.endState());
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
    updateEventExtent(s.events.at(state->eventId()), s, view_height);
  }

  intervalsToUpdate.clear();
  statesToUpdate.clear();
}
}
