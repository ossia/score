// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioValidity.hpp"

#include <Process/TimeValueSerialization.hpp>

namespace Scenario
{

ScenarioValidityChecker::~ScenarioValidityChecker() { }

bool ScenarioValidityChecker::validate(const score::DocumentContext& ctx)
{
  auto scenars = ctx.document.model().findChildren<Scenario::ProcessModel*>();
  for (auto scenar : scenars)
  {
    checkValidity(*scenar);
  }

  return true;
}

void ScenarioValidityChecker::checkValidity(const ProcessModel& scenar)
{
  using namespace ossia;

  for (const IntervalModel& interval : scenar.intervals)
  {
    auto ss = scenar.findState(interval.startState());
    SCORE_ASSERT(ss);
    auto es = scenar.findState(interval.endState());
    SCORE_ASSERT(es);

    SCORE_ASSERT(ss != es);

    SCORE_ASSERT(ss->nextInterval() == interval.id());
    SCORE_ASSERT(es->previousInterval() == interval.id());

    auto sev = scenar.findEvent(ss->eventId());
    SCORE_ASSERT(sev);
    auto eev = scenar.findEvent(es->eventId());
    SCORE_ASSERT(eev);

    SCORE_ASSERT(sev != eev);

    auto& dur = interval.duration;
    SCORE_ASSERT(dur.defaultDuration() >= 0_tv);

    SCORE_ASSERT(!dur.minDuration().infinite());
    // SCORE_ASSERT(dur.minDuration() >= 0_tv);
    SCORE_ASSERT(dur.minDuration() <= dur.maxDuration());
    SCORE_ASSERT(
        (qFuzzyCompare(dur.defaultDuration().msec(), dur.maxDuration().msec()))
        || dur.defaultDuration() < dur.maxDuration() || dur.isMaxInfinite());

    for (auto& slot : interval.smallView())
    {
      if(!slot.nodal)
      {
        SCORE_ASSERT(slot.frontProcess);
        SCORE_ASSERT(interval.processes.find(*slot.frontProcess) != interval.processes.end());
      }
      else
      {
        SCORE_ASSERT(!slot.frontProcess);
      }
    }
    /*
    if (dur.isRigid())
    {
      SCORE_ASSERT(dur.minDuration() == dur.defaultDuration());
      SCORE_ASSERT(dur.maxDuration() == dur.defaultDuration());
    }
    else
    {
      if (dur.isMinNull())
      {
      }
      if (dur.isMaxInfinite())
      {
      }
    }
    */
  }

  for (const StateModel& state : scenar.states)
  {
    auto ev = scenar.findEvent(state.eventId());
    SCORE_ASSERT(ev);
    SCORE_ASSERT(ossia::contains(ev->states(), state.id()));

    if (state.previousInterval())
    {
      auto cst = scenar.findInterval(*state.previousInterval());
      SCORE_ASSERT(cst);
      SCORE_ASSERT(cst->endState() == state.id());
    }

    if (state.nextInterval())
    {
      auto cst = scenar.findInterval(*state.nextInterval());
      SCORE_ASSERT(cst);
      SCORE_ASSERT(cst->startState() == state.id());
    }

    auto num = ossia::count_if(
        scenar.events, [&](auto& ev) { return ossia::contains(ev.states(), state.id()); });
    SCORE_ASSERT(num == 1);
  }

  for (const EventModel& event : scenar.events)
  {
    auto tn = scenar.findTimeSync(event.timeSync());
    SCORE_ASSERT(tn);
    SCORE_ASSERT(ossia::contains(tn->events(), event.id()));

    SCORE_ASSERT(!event.states().empty());
    for (auto& state : event.states())
    {
      auto st = scenar.findState(state);
      SCORE_ASSERT(st);
      /*
      if(st->eventId() != event.id())
      {
          st->setEventId(event.id());
      }*/
      SCORE_ASSERT(st->eventId() == event.id());
    }

    auto num = ossia::count_if(
        scenar.timeSyncs, [&](auto& ev) { return ossia::contains(ev.events(), event.id()); });
    SCORE_ASSERT(num == 1);
  }

  for (const TimeSyncModel& tn : scenar.timeSyncs)
  {
    SCORE_ASSERT(!tn.events().empty());
    for (auto& event : tn.events())
    {
      auto ev = scenar.findEvent(event);
      SCORE_ASSERT(ev);
      SCORE_ASSERT(ev->timeSync() == tn.id());
    }
  }
}
}
