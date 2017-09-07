// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioValidity.hpp"

namespace Scenario
{

ScenarioValidityChecker::~ScenarioValidityChecker()
{
}

bool ScenarioValidityChecker::validate(const iscore::DocumentContext& ctx)
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
#if defined(ISCORE_DEBUG)
  for (const IntervalModel& interval : scenar.intervals)
  {
    auto ss = scenar.findState(interval.startState());
    ISCORE_ASSERT(ss);
    auto es = scenar.findState(interval.endState());
    ISCORE_ASSERT(es);

    ISCORE_ASSERT(ss->nextInterval() == interval.id());
    ISCORE_ASSERT(es->previousInterval() == interval.id());
  }

  for (const StateModel& state : scenar.states)
  {
    auto ev = scenar.findEvent(state.eventId());
    ISCORE_ASSERT(ev);
    ISCORE_ASSERT(ev->states().contains(state.id()));

    if (state.previousInterval())
    {
      auto cst = scenar.findInterval(*state.previousInterval());
      ISCORE_ASSERT(cst);
      ISCORE_ASSERT(cst->endState() == state.id());
    }

    if (state.nextInterval())
    {
      auto cst = scenar.findInterval(*state.nextInterval());
      ISCORE_ASSERT(cst);
      ISCORE_ASSERT(cst->startState() == state.id());
    }

    auto num = ossia::count_if(scenar.events, [&] (auto& ev) {
        return ossia::contains(ev.states(), state.id());
    });
    ISCORE_ASSERT(num == 1);
  }

  for (const EventModel& event : scenar.events)
  {
    auto tn = scenar.findTimeSync(event.timeSync());
    ISCORE_ASSERT(tn);
    ISCORE_ASSERT(tn->events().contains(event.id()));

    // ISCORE_ASSERT(!event.states().empty());
    for (auto& state : event.states())
    {
      auto st = scenar.findState(state);
      ISCORE_ASSERT(st);
      /*
      if(st->eventId() != event.id())
      {
          st->setEventId(event.id());
      }*/
      ISCORE_ASSERT(st->eventId() == event.id());
    }

    auto num = ossia::count_if(scenar.timeSyncs, [&] (auto& ev) {
        return ossia::contains(ev.events(), event.id());
    });
    ISCORE_ASSERT(num == 1);
  }

  for (const TimeSyncModel& tn : scenar.timeSyncs)
  {
    ISCORE_ASSERT(!tn.events().empty());
    for (auto& event : tn.events())
    {
      auto ev = scenar.findEvent(event);
      ISCORE_ASSERT(ev);
      ISCORE_ASSERT(ev->timeSync() == tn.id());
    }
  }
#endif
}
}
