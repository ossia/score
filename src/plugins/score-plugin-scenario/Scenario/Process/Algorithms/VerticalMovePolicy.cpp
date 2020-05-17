// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncPresenter.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>

#include <score/model/EntityMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/detail/flat_set.hpp>

#include <limits>

namespace Scenario
{

void updateTimeSyncExtent(TimeSyncPresenter& tn)
{
  double min = std::numeric_limits<double>::max();
  double max = std::numeric_limits<double>::lowest();
  for (const EventPresenter* ev : tn.events())
  {
    if (ev->extent().top() < min)
      min = ev->extent().top();
    if (ev->extent().bottom() > max)
      max = ev->extent().bottom();
  }

  if (max - min > 3.)
  {
    min += 1.;
    max -= 1.;
  }

  tn.setExtent({min, max});
}

void updateEventExtent(ScenarioPresenter& pres, EventPresenter& ev, double view_height)
{
  if (view_height <= 2.)
    return;

  auto& s = pres.model();
  double min = std::numeric_limits<double>::max();
  double max = std::numeric_limits<double>::lowest();

  for (StatePresenter* stp : ev.states())
  {
    auto& st = stp->model();

    if (st.heightPercentage() < min)
      min = st.heightPercentage();
    if (st.heightPercentage() > max)
      max = st.heightPercentage();

    if (const auto& itv_id = st.previousInterval())
    {
      auto itv = s.intervals.find(*itv_id);
      if (itv == s.intervals.end())
        return;

      const double h = (1. + itv->getHeight()) / view_height;
      if (itv->smallViewVisible() && st.heightPercentage() + h > max)
      {
        max = st.heightPercentage() + h;
      }
    }
    if (const auto& itv_id = st.nextInterval())
    {
      auto itv = s.intervals.find(*itv_id);
      if (itv == s.intervals.end())
        return;
      const double h = (1. + itv->getHeight()) / view_height;
      if (itv->smallViewVisible() && st.heightPercentage() + h > max)
      {
        max = st.heightPercentage() + h;
      }
    }
  }

  if (Q_UNLIKELY(ev.states().empty()))
  {
    min = 0.;
    max = 0.;
  }

  ev.setExtent({min, max});
  // TODO we could maybe skip this in case where the event
  // grows ?
  updateTimeSyncExtent(pres.timeSync(ev.model().timeSync()));
}

void updateIntervalVerticalPos(
    ScenarioPresenter& pres,
    IntervalModel& itv,
    double y,
    double view_height)
{
  // TODO why isn't this a command
  if (view_height <= 2.)
    return;

  auto& s = pres.model();
  // First make the list of all the intervals to update
  static ossia::flat_set<IntervalModel*> intervalsToUpdate;
  static ossia::flat_set<StateModel*> statesToUpdate;

  if (Q_LIKELY(!itv.graphal()))
  {
    intervalsToUpdate.insert(&itv);
  }
  StateModel* rec_state = &s.state(itv.startState());

  statesToUpdate.insert(rec_state);
  while (auto prev_itv = rec_state->previousInterval())
  {
    IntervalModel* rec_cst = &s.intervals.at(*prev_itv);
    if (rec_cst->graphal())
      break;
    intervalsToUpdate.insert(rec_cst);
    statesToUpdate.insert(rec_state);
    rec_state = &s.states.at(rec_cst->startState());
  }
  statesToUpdate.insert(rec_state); // Add the first state

  rec_state = &s.state(itv.endState());
  statesToUpdate.insert(rec_state);
  while (auto next_itv = rec_state->nextInterval())
  {
    IntervalModel* rec_cst = &s.intervals.at(*next_itv);
    if (rec_cst->graphal())
      break;
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
    updateEventExtent(pres, pres.event(state->eventId()), view_height);
  }

  intervalsToUpdate.clear();
  statesToUpdate.clear();
}
}
