#pragma once
#include <Scenario/Process/ScenarioInterface.hpp>
#include <tuple>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/tools/std/IndirectContainer.hpp>
#include <iscore_plugin_scenario_export.h>

class DataStream;

class JSONObject;
class QObject;

namespace Scenario
{
class IntervalModel;
class EventModel;
class StateModel;
class TimeSyncModel;
class ISCORE_PLUGIN_SCENARIO_EXPORT BaseScenarioContainer
    : public ScenarioInterface
{
  ISCORE_SERIALIZE_FRIENDS
public:
  struct no_init
  {
  };
  explicit BaseScenarioContainer(QObject* parentObject);
  explicit BaseScenarioContainer(no_init, QObject* parentObject);
  explicit BaseScenarioContainer(
      const BaseScenarioContainer&, QObject* parentObject);

  virtual ~BaseScenarioContainer();

  QObject& parentObject() const
  {
    return *m_parent;
  }

  ElementContainer<IntervalModel> getIntervals() const final override
  {
    return {m_interval};
  }
  ElementContainer<StateModel> getStates() const final override
  {
    return {m_startState, m_endState};
  }
  ElementContainer<EventModel> getEvents() const final override
  {
    return {m_startEvent, m_endEvent};
  }
  ElementContainer<TimeSyncModel> getTimeSyncs() const final override
  {
    return {m_startNode, m_endNode};
  }

  IntervalModel*
  findInterval(const Id<IntervalModel>& id) const final override;

  EventModel* findEvent(const Id<EventModel>& id) const final override;

  TimeSyncModel*
  findTimeSync(const Id<TimeSyncModel>& id) const final override;

  StateModel* findState(const Id<StateModel>& id) const final override;

  IntervalModel&
  interval(const Id<IntervalModel>& id) const final override;

  EventModel& event(const Id<EventModel>& id) const final override;

  TimeSyncModel& timeSync(const Id<TimeSyncModel>& id) const final override;

  StateModel& state(const Id<StateModel>& id) const final override;

  IntervalModel& interval() const;

  TimeSyncModel& startTimeSync() const final override;
  TimeSyncModel& endTimeSync() const;

  EventModel& startEvent() const;
  EventModel& endEvent() const;

  StateModel& startState() const;
  StateModel& endState() const;

  iscore::IndirectArray<IntervalModel, 1> intervals() const
  {
    return m_interval;
  }
  iscore::IndirectArray<EventModel, 2> events() const
  {
    return {m_startEvent, m_endEvent};
  }
  iscore::IndirectArray<StateModel, 2> states() const
  {
    return {m_startState, m_endState};
  }
  iscore::IndirectArray<TimeSyncModel, 2> timeSyncs() const
  {
    return {m_startNode, m_endNode};
  }

protected:
  TimeSyncModel* m_startNode{};
  TimeSyncModel* m_endNode{};

  EventModel* m_startEvent{};
  EventModel* m_endEvent{};

  StateModel* m_startState{};
  StateModel* m_endState{};

  IntervalModel* m_interval{};

  auto elements() const
  {
    return std::make_tuple(
        m_startNode, m_endNode, m_startEvent, m_endEvent, m_startState,
        m_endState, m_interval);
  }

private:
  QObject* m_parent{}; // Parent for the intervals, timesyncs, etc.
                       // If inheriting, m_parent should be this.
};

inline auto intervals(const BaseScenarioContainer& scenar)
{
  return scenar.intervals();
}
inline auto events(const BaseScenarioContainer& scenar)
{
  return scenar.events();
}
inline auto timeSyncs(const BaseScenarioContainer& scenar)
{
  return scenar.timeSyncs();
}
inline auto states(const BaseScenarioContainer& scenar)
{
  return scenar.states();
}

template <>
struct ElementTraits<BaseScenarioContainer, IntervalModel>
{
  static const constexpr auto accessor
      = static_cast<iscore::IndirectArray<IntervalModel, 1> (*)(
          const BaseScenarioContainer&)>(&intervals);
};
template <>
struct ElementTraits<BaseScenarioContainer, EventModel>
{
  static const constexpr auto accessor
      = static_cast<iscore::IndirectArray<EventModel, 2> (*)(
          const BaseScenarioContainer&)>(&events);
};
template <>
struct ElementTraits<BaseScenarioContainer, TimeSyncModel>
{
  static const constexpr auto accessor
      = static_cast<iscore::IndirectArray<TimeSyncModel, 2> (*)(
          const BaseScenarioContainer&)>(&timeSyncs);
};
template <>
struct ElementTraits<BaseScenarioContainer, StateModel>
{
  static const constexpr auto accessor
      = static_cast<iscore::IndirectArray<StateModel, 2> (*)(
          const BaseScenarioContainer&)>(&states);
};
}
