#pragma once
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Instantiations.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Sequence/SequenceMetadata.hpp>

#include <score/model/EntityMap.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/selection/Selection.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score/tools/std/Optional.hpp>

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

#include <wobjectdefs.h>
namespace Sequence
{
/**
 * @brief The core hierarchical and temporal process of score
 */
class SCORE_PLUGIN_SCENARIO_EXPORT ProcessModel final
    : public Process::ProcessModel,
      public Scenario::ScenarioInterface
{
  W_OBJECT(ProcessModel)

  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Sequence::ProcessModel)
  friend class ScenarioTemporalLayerFactory;

public:
  std::unique_ptr<Process::Inlet> inlet;
  std::unique_ptr<Process::Outlet> outlet;

  ProcessModel(
      const TimeVal& duration, const Id<Process::ProcessModel>& id,
      QObject* parent);
  template <typename Impl>
  ProcessModel(Impl& vis, QObject* parent) : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  void init();

  ~ProcessModel() override;

  //// SequenceModel specifics ////

  // Accessors
  score::IndirectContainer<Scenario::IntervalModel> getIntervals() const final override
  {
    return intervals.map().as_indirect_vec();
  }

  score::IndirectContainer<Scenario::StateModel> getStates() const final override
  {
    return states.map().as_indirect_vec();
  }

  score::IndirectContainer<Scenario::EventModel> getEvents() const final override
  {
    return events.map().as_indirect_vec();
  }

  score::IndirectContainer<Scenario::TimeSyncModel> getTimeSyncs() const final override
  {
    return timeSyncs.map().as_indirect_vec();
  }

  Scenario::IntervalModel* findInterval(const Id<Scenario::IntervalModel>& id) const final override
  {
    return ossia::ptr_find(intervals, id);
  }
  Scenario::EventModel* findEvent(const Id<Scenario::EventModel>& id) const final override
  {
    return ossia::ptr_find(events, id);
  }
  Scenario::TimeSyncModel* findTimeSync(const Id<Scenario::TimeSyncModel>& id) const final override
  {
    return ossia::ptr_find(timeSyncs, id);
  }
  Scenario::StateModel* findState(const Id<Scenario::StateModel>& id) const final override
  {
    return ossia::ptr_find(states, id);
  }

  Scenario::IntervalModel&
  interval(const Id<Scenario::IntervalModel>& intervalId) const final override
  {
    return intervals.at(intervalId);
  }
  Scenario::EventModel& event(const Id<Scenario::EventModel>& eventId) const final override
  {
    return events.at(eventId);
  }
  Scenario::TimeSyncModel&
  timeSync(const Id<Scenario::TimeSyncModel>& timeSyncId) const final override
  {
    return timeSyncs.at(timeSyncId);
  }
  Scenario::StateModel& state(const Id<Scenario::StateModel>& stId) const final override
  {
    return states.at(stId);
  }
  Scenario::TimeSyncModel& startTimeSync() const final override
  {
    return timeSyncs.at(m_startTimeSyncId);
  }

  Scenario::EventModel& startEvent() const
  {
    return events.at(m_startEventId);
  }

  score::EntityMap<Scenario::IntervalModel> intervals;
  score::EntityMap<Scenario::EventModel> events;
  score::EntityMap<Scenario::TimeSyncModel> timeSyncs;
  score::EntityMap<Scenario::StateModel> states;

public:
  void stateMoved(const Scenario::StateModel& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, stateMoved, arg_1);
  void eventMoved(const Scenario::EventModel& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, eventMoved, arg_1);
  void intervalMoved(const Scenario::IntervalModel& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, intervalMoved, arg_1);

  void locked() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, locked);
  void unlocked() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, unlocked);

public:
  void lock()
  {
    locked();
  };
  W_SLOT(lock)
  void unlock()
  {
    unlocked();
  };
  W_SLOT(unlock)

private:
  //// ProcessModel specifics ////
  void setDurationAndScale(const TimeVal& newDuration) noexcept override;
  void setDurationAndGrow(const TimeVal& newDuration) noexcept override;
  void setDurationAndShrink(const TimeVal& newDuration) noexcept override;

  void startExecution() override;
  void stopExecution() override;
  void reset() override;

  Selection selectableChildren() const noexcept override;

public:
  Selection selectedChildren() const noexcept override;

private:
  void setSelection(const Selection& s) const noexcept override;
  bool event(QEvent* e) override
  {
    return QObject::event(e);
  }

  bool contentHasDuration() const noexcept override;
  TimeVal contentDuration() const noexcept override;

  template <typename Fun>
  void apply(Fun fun) const
  {
    fun(&ProcessModel::intervals);
    fun(&ProcessModel::states);
    fun(&ProcessModel::events);
    fun(&ProcessModel::timeSyncs);
  }

  Id<Scenario::TimeSyncModel> m_startTimeSyncId{};
  Id<Scenario::EventModel> m_startEventId{};
  Id<Scenario::StateModel> m_startStateId{};
  // By default, creation in the void will make a interval
  // that goes to the startEvent and add a new state
};
}
namespace Scenario
{
inline auto& intervals(const Sequence::ProcessModel& scenar)
{
  return scenar.intervals;
}
inline auto& events(const Sequence::ProcessModel& scenar)
{
  return scenar.events;
}
inline auto& timeSyncs(const Sequence::ProcessModel& scenar)
{
  return scenar.timeSyncs;
}
inline auto& states(const Sequence::ProcessModel& scenar)
{
  return scenar.states;
}

template <>
struct ElementTraits<Sequence::ProcessModel, Scenario::IntervalModel>
{
  static const constexpr auto accessor = static_cast<const score::EntityMap<
      Scenario::IntervalModel>& (*)(const Sequence::ProcessModel&)>(&intervals);
};
template <>
struct ElementTraits<Sequence::ProcessModel, Scenario::EventModel>
{
  static const constexpr auto accessor = static_cast<
      const score::EntityMap<Scenario::EventModel>& (*)(const Sequence::ProcessModel&)>(
      &events);
};
template <>
struct ElementTraits<Sequence::ProcessModel, Scenario::TimeSyncModel>
{
  static const constexpr auto accessor = static_cast<const score::EntityMap<
      Scenario::TimeSyncModel>& (*)(const Sequence::ProcessModel&)>(&timeSyncs);
};
template <>
struct ElementTraits<Sequence::ProcessModel, Scenario::StateModel>
{
  static const constexpr auto accessor = static_cast<
      const score::EntityMap<Scenario::StateModel>& (*)(const Sequence::ProcessModel&)>(
      &states);
};
}
DESCRIPTION_METADATA(
    SCORE_PLUGIN_SCENARIO_EXPORT, Sequence::ProcessModel, "Sequence")

W_REGISTER_ARGTYPE(const Sequence::ProcessModel&)
W_REGISTER_ARGTYPE(Sequence::ProcessModel&)
W_REGISTER_ARGTYPE(Sequence::ProcessModel)
