#pragma once
#include <iscore/model/Identifier.hpp>
#include <iscore/tools/std/IndirectContainer.hpp>
#include <iscore_plugin_scenario_export.h>
namespace Scenario
{
class ConstraintModel;
class EventModel;
class StateModel;
class TimeSyncModel;
template <typename T>
using ElementContainer = iscore::IndirectContainer<std::vector, T>;

class ISCORE_PLUGIN_SCENARIO_EXPORT ScenarioInterface
{
public:
  virtual ~ScenarioInterface();
  virtual ConstraintModel*
  findConstraint(const Id<ConstraintModel>& constraintId) const = 0;
  virtual EventModel* findEvent(const Id<EventModel>& eventId) const = 0;
  virtual TimeSyncModel*
  findTimeSync(const Id<TimeSyncModel>& timeSyncId) const = 0;
  virtual StateModel* findState(const Id<StateModel>& stId) const = 0;

  virtual ConstraintModel&
  constraint(const Id<ConstraintModel>& constraintId) const = 0;
  virtual EventModel& event(const Id<EventModel>& eventId) const = 0;
  virtual TimeSyncModel&
  timeSync(const Id<TimeSyncModel>& timeSyncId) const = 0;
  virtual StateModel& state(const Id<StateModel>& stId) const = 0;

  virtual ElementContainer<ConstraintModel> getConstraints() const = 0;
  virtual ElementContainer<StateModel> getStates() const = 0;
  virtual ElementContainer<EventModel> getEvents() const = 0;
  virtual ElementContainer<TimeSyncModel> getTimeSyncs() const = 0;

  virtual TimeSyncModel& startTimeSync() const = 0;
};

static inline auto startId_val()
{
  return 0;
}

static inline auto endId_val()
{
  return 1;
}
template <typename T>
static auto startId()
{
  return Id<T>{startId_val()};
}
template <typename T>
static auto endId()
{
  return Id<T>{endId_val()};
}

template <typename Scenario_T, typename Element_T>
struct ElementTraits;
// { auto container_accessor = &constraints; etc... }

template <>
struct ElementTraits<Scenario::ScenarioInterface, ConstraintModel>
{
  static const constexpr auto accessor = &ScenarioInterface::getConstraints;
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
