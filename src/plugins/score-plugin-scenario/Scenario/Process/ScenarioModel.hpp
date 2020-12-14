#pragma once
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Instantiations.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Process/ScenarioProcessMetadata.hpp>

#include <score/model/EntityMap.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/selection/Selection.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score/tools/std/Optional.hpp>

#include <QList>
#include <QObject>
#include <QVector>

#include <verdigris>
namespace Scenario
{
struct TimenodeGraph;

/**
 * @brief The core hierarchical and temporal process of score
 */
class SCORE_PLUGIN_SCENARIO_EXPORT ProcessModel final : public Process::ProcessModel,
                                                        public ScenarioInterface
{
  W_OBJECT(ProcessModel)

  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Scenario::ProcessModel)
  friend class ScenarioTemporalLayerFactory;

public:
  std::unique_ptr<Process::AudioInlet> inlet;
  std::unique_ptr<Process::AudioOutlet> outlet;

  ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      const score::DocumentContext& ctx,
      QObject* parent);
  template <typename Impl>
  ProcessModel(Impl& vis, const score::DocumentContext& ctx, QObject* parent)
      : Process::ProcessModel{vis, parent}, m_context{ctx}
  {
    vis.writeTo(*this);
    init();
  }

  const score::DocumentContext& context() const noexcept { return m_context; }
  void init();
  bool hasCycles() const noexcept;

  ~ProcessModel() override;

  //// ScenarioModel specifics ////

  // Accessors
  score::IndirectContainer<IntervalModel> getIntervals() const final override
  {
    return intervals.map().as_indirect_vec();
  }

  score::IndirectContainer<StateModel> getStates() const final override
  {
    return states.map().as_indirect_vec();
  }

  score::IndirectContainer<EventModel> getEvents() const final override
  {
    return events.map().as_indirect_vec();
  }

  score::IndirectContainer<TimeSyncModel> getTimeSyncs() const final override
  {
    return timeSyncs.map().as_indirect_vec();
  }

  IntervalModel* findInterval(const Id<IntervalModel>& id) const final override
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

  IntervalModel& interval(const Id<IntervalModel>& intervalId) const final override
  {
    return intervals.at(intervalId);
  }
  EventModel& event(const Id<EventModel>& eventId) const final override
  {
    return events.at(eventId);
  }
  TimeSyncModel& timeSync(const Id<TimeSyncModel>& timeSyncId) const final override
  {
    return timeSyncs.at(timeSyncId);
  }
  StateModel& state(const Id<StateModel>& stId) const final override { return states.at(stId); }
  CommentBlockModel& comment(const Id<CommentBlockModel>& cmtId) const
  {
    return comments.at(cmtId);
  }

  TimeSyncModel& startTimeSync() const { return timeSyncs.at(m_startTimeSyncId); }

  EventModel& startEvent() const { return events.at(m_startEventId); }

  score::EntityMap<IntervalModel> intervals;
  score::EntityMap<EventModel> events;
  score::EntityMap<TimeSyncModel> timeSyncs;
  score::EntityMap<StateModel> states;
  score::EntityMap<CommentBlockModel> comments;

public:
  void intervalMoved(const Scenario::IntervalModel& arg_1) const
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, intervalMoved, arg_1)

  void locked() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, locked)
  void unlocked() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, unlocked)

public:
  void lock() { locked(); };
  W_SLOT(lock)
  void unlock() { unlocked(); };
  W_SLOT(unlock)

private:
  //// ProcessModel specifics ////
  void setDurationAndScale(const TimeVal& newDuration) noexcept override;
  void setDurationAndGrow(const TimeVal& newDuration) noexcept override;
  void setDurationAndShrink(const TimeVal& newDuration) noexcept override;

  void ancestorStartDateChanged() override;
  void ancestorTempoChanged() override;

  Selection selectableChildren() const noexcept override;

public:
  Selection selectedChildren() const noexcept override;

private:
  void setSelection(const Selection& s) const noexcept override;
  bool event(QEvent* e) override { return QObject::event(e); }

  TimeVal contentDuration() const noexcept override;

  template <typename Fun>
  void apply(Fun fun) const
  {
    fun(&ProcessModel::intervals);
    fun(&ProcessModel::states);
    fun(&ProcessModel::events);
    fun(&ProcessModel::timeSyncs);
    fun(&ProcessModel::comments);
  }
  const score::DocumentContext& m_context;

  Id<TimeSyncModel> m_startTimeSyncId{};
  Id<EventModel> m_startEventId{};
  Id<StateModel> m_startStateId{};
  // By default, creation in the void will make a interval
  // that goes to the startEvent and add a new state

  std::unique_ptr<TimenodeGraph> m_graph;
};
}
// TODO this ought to go in Selection.hpp ?
template <typename Vector>
std::vector<const typename Vector::value_type*> selectedElements(const Vector& in)
{
  std::vector<const typename Vector::value_type*> out;
  for (const auto& elt : in)
  {
    if (elt.selection.get())
      out.push_back(&elt);
  }

  return out;
}

template <typename T, typename Container>
std::vector<const T*> filterSelectionByType(const Container& sel)
{
  std::vector<const T*> selected_elements;
  for (auto obj : sel)
  {
    // TODO replace with a virtual Element::type() which will be faster.
    if (auto casted_obj = qobject_cast<const T*>(obj.data()))
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
intervalsBeforeTimeSync(const Scenario::ProcessModel&, const Id<TimeSyncModel>& timeSyncId);

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
      = static_cast<const score::EntityMap<IntervalModel>& (*)(const Scenario::ProcessModel&)>(
          &intervals);
};
template <>
struct ElementTraits<Scenario::ProcessModel, EventModel>
{
  static const constexpr auto accessor
      = static_cast<const score::EntityMap<EventModel>& (*)(const Scenario::ProcessModel&)>(
          &events);
};
template <>
struct ElementTraits<Scenario::ProcessModel, TimeSyncModel>
{
  static const constexpr auto accessor
      = static_cast<const score::EntityMap<TimeSyncModel>& (*)(const Scenario::ProcessModel&)>(
          &timeSyncs);
};
template <>
struct ElementTraits<Scenario::ProcessModel, StateModel>
{
  static const constexpr auto accessor
      = static_cast<const score::EntityMap<StateModel>& (*)(const Scenario::ProcessModel&)>(
          &states);
};
}
DESCRIPTION_METADATA(SCORE_PLUGIN_SCENARIO_EXPORT, Scenario::ProcessModel, "Scenario")

W_REGISTER_ARGTYPE(const Scenario::ProcessModel&)
W_REGISTER_ARGTYPE(Scenario::ProcessModel&)
W_REGISTER_ARGTYPE(Scenario::ProcessModel)
