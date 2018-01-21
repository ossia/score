#pragma once
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Process/ScenarioProcessMetadata.hpp>
#include <boost/iterator/indirect_iterator.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/tools/std/Optional.hpp>

#include <score/selection/Selection.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score/model/Identifier.hpp>

class DataStream;
class JSONObject;
class QEvent;

namespace Scenario
{
/**
 * @brief The core hierarchical and temporal process of score
 */
class SCORE_PLUGIN_SCENARIO_EXPORT ProcessModel final
    : public Process::ProcessModel,
      public ScenarioInterface
{
  Q_OBJECT

  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Scenario::ProcessModel)
  friend class ScenarioTemporalLayerFactory;

public:
  std::unique_ptr<Process::Inlet> inlet;
  std::unique_ptr<Process::Outlet> outlet;

  ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);
  template <typename Impl>
  ProcessModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }
  void init()
  {
    m_inlets.push_back(inlet.get());
    m_outlets.push_back(outlet.get());
  }
  ~ProcessModel() override;

  //// ScenarioModel specifics ////

  // Accessors
  ElementContainer<IntervalModel> getIntervals() const final override
  {
    auto& map = intervals.map().get();
    return {map.begin(), map.end()};
  }

  ElementContainer<StateModel> getStates() const final override
  {
    auto& map = states.map().get();
    return {map.begin(), map.end()};
  }

  ElementContainer<EventModel> getEvents() const final override
  {
    auto& map = events.map().get();
    return {map.begin(), map.end()};
  }

  ElementContainer<TimeSyncModel> getTimeSyncs() const final override
  {
    auto& map = timeSyncs.map().get();
    return {map.begin(), map.end()};
  }

  IntervalModel*
  findInterval(const Id<IntervalModel>& id) const final override
  {
    return ossia::ptr_find(intervals, id);
  }
  EventModel* findEvent(const Id<EventModel>& id) const final override
  {
    return ossia::ptr_find(events, id);
  }
  TimeSyncModel* findTimeSync(const Id<TimeSyncModel>& id) const final override
  {
    return ossia::ptr_find(timeSyncs, id);
  }
  StateModel* findState(const Id<StateModel>& id) const final override
  {
    return ossia::ptr_find(states, id);
  }

  IntervalModel&
  interval(const Id<IntervalModel>& intervalId) const final override
  {
    return intervals.at(intervalId);
  }
  EventModel& event(const Id<EventModel>& eventId) const final override
  {
    return events.at(eventId);
  }
  TimeSyncModel&
  timeSync(const Id<TimeSyncModel>& timeSyncId) const final override
  {
    return timeSyncs.at(timeSyncId);
  }
  StateModel& state(const Id<StateModel>& stId) const final override
  {
    return states.at(stId);
  }
  CommentBlockModel& comment(const Id<CommentBlockModel>& cmtId) const
  {
    return comments.at(cmtId);
  }

  TimeSyncModel& startTimeSync() const final override
  {
    return timeSyncs.at(m_startTimeSyncId);
  }

  EventModel& startEvent() const
  {
    return events.at(m_startEventId);
  }

  score::EntityMap<IntervalModel> intervals;
  score::EntityMap<EventModel> events;
  score::EntityMap<TimeSyncModel> timeSyncs;
  score::EntityMap<StateModel> states;
  score::EntityMap<CommentBlockModel> comments;

Q_SIGNALS:
  void stateMoved(const Scenario::StateModel&);
  void eventMoved(const Scenario::EventModel&);
  void intervalMoved(const Scenario::IntervalModel&);
  void commentMoved(const Scenario::CommentBlockModel&);

  void locked();
  void unlocked();

public Q_SLOTS:
  void lock()
  {
    emit locked();
  }
  void unlock()
  {
    emit unlocked();
  }

private:
  //// ProcessModel specifics ////
  void setDurationAndScale(const TimeVal& newDuration) override;
  void setDurationAndGrow(const TimeVal& newDuration) override;
  void setDurationAndShrink(const TimeVal& newDuration) override;

  void startExecution() override;
  void stopExecution() override;
  void reset() override;

  Selection selectableChildren() const override;

public:
  Selection selectedChildren() const override;

private:
  void setSelection(const Selection& s) const override;
  bool event(QEvent* e) override
  {
    return QObject::event(e);
  }

  bool contentHasDuration() const override;
  TimeVal contentDuration() const override;

  template <typename Fun>
  void apply(Fun fun) const
  {
    fun(&ProcessModel::intervals);
    fun(&ProcessModel::states);
    fun(&ProcessModel::events);
    fun(&ProcessModel::timeSyncs);
    fun(&ProcessModel::comments);
  }

  Id<TimeSyncModel> m_startTimeSyncId{};
  Id<EventModel> m_startEventId{};
  Id<StateModel> m_startStateId{};
  // By default, creation in the void will make a interval
  // that goes to the startEvent and add a new state
};
}
// TODO this ought to go in Selection.hpp ?
template <typename Vector>
QList<const typename Vector::value_type*> selectedElements(const Vector& in)
{
  QList<const typename Vector::value_type*> out;
  for (const auto& elt : in)
  {
    if (elt.selection.get())
      out.append(&elt);
  }

  return out;
}

template <typename T, typename Container>
QList<const T*> filterSelectionByType(const Container& sel)
{
  QList<const T*> selected_elements;
  for (auto obj : sel)
  {
    // TODO replace with a virtual Element::type() which will be faster.
    if (auto casted_obj = dynamic_cast<const T*>(obj.data()))
    {
      if (casted_obj->selection.get())
      {
        selected_elements.push_back(casted_obj);
      }
    }
  }

  return selected_elements;
}

namespace Scenario
{
SCORE_PLUGIN_SCENARIO_EXPORT const QVector<Id<IntervalModel>>
intervalsBeforeTimeSync(
    const Scenario::ProcessModel&, const Id<TimeSyncModel>& timeSyncId);

const StateModel*
furthestSelectedState(const Scenario::ProcessModel& scenario);
const StateModel* furthestSelectedStateWithoutFollowingInterval(
    const Scenario::ProcessModel& scenario);

inline auto& intervals(const Scenario::ProcessModel& scenar)
{
  return scenar.intervals;
}
inline auto& events(const Scenario::ProcessModel& scenar)
{
  return scenar.events;
}
inline auto& timeSyncs(const Scenario::ProcessModel& scenar)
{
  return scenar.timeSyncs;
}
inline auto& states(const Scenario::ProcessModel& scenar)
{
  return scenar.states;
}

template <>
struct ElementTraits<Scenario::ProcessModel, IntervalModel>
{
  static const constexpr auto accessor
      = static_cast<const score::EntityMap<IntervalModel>& (*)(const Scenario::
                                                              ProcessModel&)>(
          &intervals);
};
template <>
struct ElementTraits<Scenario::ProcessModel, EventModel>
{
  static const constexpr auto accessor
      = static_cast<const score::EntityMap<EventModel>& (*)(const Scenario::
                                                         ProcessModel&)>(
          &events);
};
template <>
struct ElementTraits<Scenario::ProcessModel, TimeSyncModel>
{
  static const constexpr auto accessor
      = static_cast<const score::EntityMap<TimeSyncModel>& (*)(const Scenario::
                                                            ProcessModel&)>(
          &timeSyncs);
};
template <>
struct ElementTraits<Scenario::ProcessModel, StateModel>
{
  static const constexpr auto accessor
      = static_cast<const score::EntityMap<StateModel>& (*)(const Scenario::
                                                         ProcessModel&)>(
          &states);
};
}
DESCRIPTION_METADATA(
    SCORE_PLUGIN_SCENARIO_EXPORT, Scenario::ProcessModel, "Scenario")
