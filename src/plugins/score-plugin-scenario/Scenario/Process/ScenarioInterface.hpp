#pragma once
#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>

#include <score/model/Identifier.hpp>
#include <score/tools/std/IndirectContainer.hpp>

#include <score_plugin_scenario_export.h>

#include <verdigris>
class OngoingCommandDispatcher;
namespace Scenario
{
class IntervalModel;
class EventModel;
class StateModel;
class TimeSyncModel;

class SCORE_PLUGIN_SCENARIO_EXPORT ScenarioInterface
{
public:
  virtual ~ScenarioInterface();
  virtual IntervalModel* findInterval(const Id<IntervalModel>& intervalId) const = 0;
  virtual EventModel* findEvent(const Id<EventModel>& eventId) const = 0;
  virtual TimeSyncModel* findTimeSync(const Id<TimeSyncModel>& timeSyncId) const = 0;
  virtual StateModel* findState(const Id<StateModel>& stId) const = 0;

  virtual IntervalModel& interval(const Id<IntervalModel>& intervalId) const = 0;
  virtual EventModel& event(const Id<EventModel>& eventId) const = 0;
  virtual TimeSyncModel& timeSync(const Id<TimeSyncModel>& timeSyncId) const = 0;
  virtual StateModel& state(const Id<StateModel>& stId) const = 0;

  virtual score::IndirectContainer<IntervalModel> getIntervals() const = 0;
  virtual score::IndirectContainer<StateModel> getStates() const = 0;
  virtual score::IndirectContainer<EventModel> getEvents() const = 0;
  virtual score::IndirectContainer<TimeSyncModel> getTimeSyncs() const = 0;
};

static const constexpr auto startId_val = 0;
static const constexpr auto endId_val = 1;

template <typename T>
auto startId()
{
  return Id<T>{startId_val};
}

template <typename T>
auto endId()
{
  return Id<T>{endId_val};
}

template <typename Scenario_T, typename Element_T>
struct ElementTraits;
// { auto container_accessor = &intervals; etc... }

template <>
struct ElementTraits<Scenario::ScenarioInterface, IntervalModel>
{
  static const constexpr auto accessor = &ScenarioInterface::getIntervals;
};
template <>
struct ElementTraits<Scenario::ScenarioInterface, EventModel>
{
  static const constexpr auto accessor = &ScenarioInterface::getEvents;
};
template <>
struct ElementTraits<Scenario::ScenarioInterface, TimeSyncModel>
{
  static const constexpr auto accessor = &ScenarioInterface::getTimeSyncs;
};
template <>
struct ElementTraits<Scenario::ScenarioInterface, StateModel>
{
  static const constexpr auto accessor = &ScenarioInterface::getStates;
};
}

W_REGISTER_ARGTYPE(const Scenario::ScenarioInterface&)
W_REGISTER_ARGTYPE(Scenario::ScenarioInterface)
