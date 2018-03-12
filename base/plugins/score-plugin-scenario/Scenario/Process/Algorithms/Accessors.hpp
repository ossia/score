#pragma once
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
// Intervals
namespace Scenario
{
template <typename Scenario_T>
StateModel& startState(const IntervalModel& cst, const Scenario_T& scenario)
{
  return scenario.state(cst.startState());
}

template <typename Scenario_T>
StateModel& endState(const IntervalModel& cst, const Scenario_T& scenario)
{
  return scenario.state(cst.endState());
}

template <typename Scenario_T>
const EventModel&
startEvent(const IntervalModel& cst, const Scenario_T& scenario)
{
  return scenario.event(startState(cst, scenario).eventId());
}

template <typename Scenario_T>
const EventModel&
endEvent(const IntervalModel& cst, const Scenario_T& scenario)
{
  return scenario.event(endState(cst, scenario).eventId());
}

template <typename Scenario_T>
const TimeSyncModel&
startTimeSync(const IntervalModel& cst, const Scenario_T& scenario)
{
  return scenario.timeSync(startEvent(cst, scenario).timeSync());
}

template <typename Scenario_T>
const TimeSyncModel&
endTimeSync(const IntervalModel& cst, const Scenario_T& scenario)
{
  return scenario.timeSync(endEvent(cst, scenario).timeSync());
}

// Events
template <typename Scenario_T>
const TimeSyncModel&
parentTimeSync(const EventModel& ev, const Scenario_T& scenario)
{
  return scenario.timeSync(ev.timeSync());
}

// States
template <typename Scenario_T>
const EventModel& parentEvent(const StateModel& st, const Scenario_T& scenario)
{
  return scenario.event(st.eventId());
}

template <typename Scenario_T>
const TimeSyncModel&
parentTimeSync(const StateModel& st, const Scenario_T& scenario)
{
  return parentTimeSync(parentEvent(st, scenario), scenario);
}

// This one is just here to allow generic facilities
template <typename Scenario_T>
const TimeSyncModel& parentTimeSync(const TimeSyncModel& st, const Scenario_T&)
{
  return st;
}

template <typename Scenario_T>
const IntervalModel&
previousInterval(const StateModel& st, const Scenario_T& scenario)
{
  SCORE_ASSERT(st.previousInterval());
  return scenario.interval(*st.previousInterval());
}

template <typename Scenario_T>
const IntervalModel&
nextInterval(const StateModel& st, const Scenario_T& scenario)
{
  SCORE_ASSERT(st.nextInterval());
  return scenario.interval(*st.nextInterval());
}

template <typename Scenario_T>
std::list<Id<IntervalModel>> nextIntervals(const EventModel& ev, const Scenario_T& scenario)
{
  std::list<Id<IntervalModel>> intervals;
  for (const Id<StateModel>& state : ev.states())
  {
    const StateModel& st = scenario.state(state);
    if (const auto& cst_id = st.nextInterval())
      intervals.push_back(*cst_id);
  }
  return intervals;
}
template <typename Scenario_T>
std::list<Id<IntervalModel>> previousIntervals(const EventModel& ev, const Scenario_T& scenario)
{
  std::list<Id<IntervalModel>> intervals;
  for (const Id<StateModel>& state : ev.states())
  {
    const StateModel& st = scenario.state(state);
    if (const auto& cst_id = st.previousInterval())
      intervals.push_back(*cst_id);
  }
  return intervals;
}

template <typename Scenario_T>
bool hasPreviousIntervals(const EventModel& ev, const Scenario_T& scenario)
{
  for (const Id<StateModel>& state : ev.states())
  {
    const StateModel& st = scenario.state(state);
    if (st.previousInterval())
      return true;
  }
  return false;
}

// TimeSyncs
template <typename Scenario_T>
std::list<Id<IntervalModel>> nextIntervals(const TimeSyncModel& tn, const Scenario_T& scenario)
{
  std::list<Id<IntervalModel>> intervals;
  for (const Id<EventModel>& event_id : tn.events())
  {
    const EventModel& event = scenario.event(event_id);
    auto next = nextIntervals(event, scenario);
    intervals.splice(intervals.end(), next);
  }

  return intervals;
}

template <typename Scenario_T>
std::list<Id<IntervalModel>> previousIntervals(const TimeSyncModel& tn, const Scenario_T& scenario)
{
  std::list<Id<IntervalModel>> intervals;
  for (const Id<EventModel>& event_id : tn.events())
  {
    const EventModel& event = scenario.event(event_id);
    auto prev = previousIntervals(event, scenario);
    intervals.splice(intervals.end(), prev);
  }

  return intervals;
}

template <typename Scenario_T>
bool hasPreviousIntervals(const TimeSyncModel& tn, const Scenario_T& scenario)
{
  for (const Id<EventModel>& event_id : tn.events())
  {
    const EventModel& event = scenario.event(event_id);
    if(hasPreviousIntervals(event, scenario))
      return true;
  }

  return false;
}
template <typename Scenario_T>
std::list<Id<StateModel>> states(const TimeSyncModel& tn, const Scenario_T& scenario)
{
  std::list<Id<StateModel>> stateList;
  for (const Id<EventModel>& event_id : tn.events())
  {
    const EventModel& event = scenario.event(event_id);
    std::list<Id<StateModel>> st{event.states().begin(), event.states().end()};

    stateList.splice(stateList.end(), st);
  }

  return stateList;
}

// Dates
template <typename Element_T, typename Scenario_T>
const TimeVal& date(const Element_T& e, const Scenario_T& scenario)
{
  return parentTimeSync(e, scenario).date();
}
template<typename Scenario_T>
inline const TimeVal& date(const IntervalModel& e, const Scenario_T& scenario)
{
  return e.date();
}

template <typename Element_T>
Scenario::ScenarioInterface& parentScenario(Element_T&& e)
{
  auto s = qobject_cast<Scenario::ProcessModel*>(e.parent());
  if(s)
    return *s;

  return *dynamic_cast<Scenario::ScenarioInterface*>(e.parent());
}
}
