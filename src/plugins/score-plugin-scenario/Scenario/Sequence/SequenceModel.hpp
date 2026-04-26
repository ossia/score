#pragma once
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Instantiations.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Sequence/SequenceProcessMetadata.hpp>

#include <score/model/EntityMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/selection/Selection.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <State/Address.hpp>

#include <ossia/network/value/value.hpp>

#include <QList>
#include <QMap>
#include <QSet>
#include <QVector>

#include <verdigris>

namespace Sequence
{

class SCORE_PLUGIN_SCENARIO_EXPORT SequenceModel final
    : public Process::ProcessModel
    , public Scenario::ScenarioInterface
{
  W_OBJECT(SequenceModel)

  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(SequenceModel)

public:
  // ScenarioInterface entity maps
  score::EntityMap<Scenario::IntervalModel> intervals;
  score::EntityMap<Scenario::EventModel> events;
  score::EntityMap<Scenario::TimeSyncModel> timeSyncs;
  score::EntityMap<Scenario::StateModel> states;

  SequenceModel(
      const TimeVal& duration, const Id<Process::ProcessModel>& id,
      const score::DocumentContext& ctx, QObject* parent);

  template <typename Impl>
  SequenceModel(Impl& vis, const score::DocumentContext& ctx, QObject* parent)
      : Process::ProcessModel{vis, parent}
      , m_context{ctx}
  {
    vis.writeTo(*this);
    if(auto* itv = qobject_cast<Scenario::IntervalModel*>(parent))
    {
      m_parentStartStateId = itv->startState();
      m_parentEndStateId = itv->endState();
    }
  }

  ~SequenceModel() override;

  const score::DocumentContext& context() const noexcept { return m_context; }

  // ---- ScenarioInterface ----
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
    if(auto it = intervals.map().m_map.find(id); it != intervals.map().m_map.end())
      return it->second;
    return nullptr;
  }
  Scenario::EventModel* findEvent(const Id<Scenario::EventModel>& id) const final override
  {
    if(auto it = events.map().m_map.find(id); it != events.map().m_map.end())
      return it->second;
    return nullptr;
  }
  Scenario::TimeSyncModel* findTimeSync(const Id<Scenario::TimeSyncModel>& id) const final override
  {
    if(auto it = timeSyncs.map().m_map.find(id); it != timeSyncs.map().m_map.end())
      return it->second;
    return nullptr;
  }
  Scenario::StateModel* findState(const Id<Scenario::StateModel>& id) const final override
  {
    if(auto it = states.map().m_map.find(id); it != states.map().m_map.end())
      return it->second;
    return nullptr;
  }

  Scenario::IntervalModel& interval(const Id<Scenario::IntervalModel>& id) const final override
  {
    return intervals.at(id);
  }
  Scenario::EventModel& event(const Id<Scenario::EventModel>& id) const final override
  {
    return events.at(id);
  }
  Scenario::TimeSyncModel& timeSync(const Id<Scenario::TimeSyncModel>& id) const final override
  {
    return timeSyncs.at(id);
  }
  Scenario::StateModel& state(const Id<Scenario::StateModel>& id) const final override
  {
    return states.at(id);
  }

  // ---- Sequence-specific ----

  // Holds IDs of entities created by appendSection; used by AppendSequenceSection command for undo.
  struct AppendedSection
  {
    Id<Scenario::TimeSyncModel> prevEndTimeSyncId;
    Id<Scenario::EventModel> prevEndEventId;
    Id<Scenario::TimeSyncModel> newEndTimeSyncId;
    Id<Scenario::EventModel> newEndEventId;
    Id<Scenario::StateModel> newEndStateId;
    Id<Scenario::IntervalModel> newIntervalId;
    TimeVal prevDuration;
  };

  // Parent boundary state IDs (in the parent scenario; not serialized)
  const Id<Scenario::StateModel>& parentStartStateId() const noexcept
  {
    return m_parentStartStateId;
  }
  const Id<Scenario::StateModel>& parentEndStateId() const noexcept
  {
    return m_parentEndStateId;
  }

  // Boundary timesync IDs (structural connectors for execution)
  const Id<Scenario::TimeSyncModel>& startTimeSyncId() const noexcept
  {
    return m_startTimeSyncId;
  }
  const Id<Scenario::TimeSyncModel>& endTimeSyncId() const noexcept { return m_endTimeSyncId; }
  const Id<Scenario::EventModel>& endEventId() const noexcept { return m_endEventId; }

  // Returns the state attached to a given timeSync (first event's first state)
  Id<Scenario::StateModel> stateForTimeSync(const Id<Scenario::TimeSyncModel>& tsId) const;
  // Returns the interval arriving at a timeSync (left adjacent)
  std::optional<Id<Scenario::IntervalModel>> intervalBefore(
      const Id<Scenario::TimeSyncModel>& tsId) const;
  // Returns the interval leaving a timeSync (right adjacent)
  std::optional<Id<Scenario::IntervalModel>> intervalAfter(
      const Id<Scenario::TimeSyncModel>& tsId) const;

  // Ordered helpers (left-to-right)
  QVector<Id<Scenario::TimeSyncModel>> orderedTimeSyncs() const;
  QVector<Id<Scenario::IntervalModel>> orderedIntervals() const;

  // Namespace management
  const QList<State::AddressAccessor>& parameterNamespace() const noexcept { return m_namespace; }
  void addParameter(const State::AddressAccessor& addr, const ossia::value& currentVal);
  void removeParameter(const State::AddressAccessor& addr);
  void mergeNamespace(const QList<State::AddressAccessor>& addrs);

  // Linear structure mutation
  // Appends a new section of `duration`; returns IDs of created entities (for undo).
  AppendedSection appendSection(const TimeVal& duration);
  // Reverses appendSection using the returned AppendedSection.
  void undoAppendSection(const AppendedSection& info);

  // Resizes adjacent intervals when an intermediate IS is moved
  void moveIS(const Id<Scenario::TimeSyncModel>& tsId, const TimeVal& newDate);

  // IS value management
  void setISValue(
      const Id<Scenario::TimeSyncModel>& tsId, const State::AddressAccessor& addr,
      const ossia::value& val);
  void freezeISParam(
      const Id<Scenario::TimeSyncModel>& tsId, const State::AddressAccessor& addr,
      bool frozen);

  // Frozen parameter query
  bool isParamFrozen(
      const Id<Scenario::TimeSyncModel>& tsId,
      const State::AddressAccessor& addr) const;

  // ProcessModel overrides
  TimeVal contentDuration() const noexcept override;
  void setDurationAndScale(const TimeVal& newDuration) noexcept override;
  void setDurationAndGrow(const TimeVal& newDuration) noexcept override;
  void setDurationAndShrink(const TimeVal& newDuration) noexcept override;

  Selection selectableChildren() const noexcept override;
  Selection selectedChildren() const noexcept override;

  // Signals
  void namespaceChanged() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, namespaceChanged)
  void structureChanged() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, structureChanged)

private:
  bool event(QEvent* e) override { return QObject::event(e); }
  void setSelection(const Selection& s) const noexcept override;

  // Internal helpers
  void syncAutomationEndpoints(
      const Id<Scenario::TimeSyncModel>& tsId, const State::AddressAccessor& addr);
  void rebuildAutomations(const State::AddressAccessor& addr);

  const score::DocumentContext& m_context;

  // Ordered list of tracked parameters (defines slot layout order)
  QList<State::AddressAccessor> m_namespace;

  // Freeze map: timeSyncId → set of frozen parameter addresses
  QMap<Id<Scenario::TimeSyncModel>, QSet<State::AddressAccessor>> m_frozenParams;

  // IDs of the boundary timeSyncs (structural, not user-visible IS)
  Id<Scenario::TimeSyncModel> m_startTimeSyncId{};
  Id<Scenario::EventModel> m_startEventId{};

  Id<Scenario::TimeSyncModel> m_endTimeSyncId{};
  Id<Scenario::EventModel> m_endEventId{};

  // IDs of the parent interval's start/end states (in the parent scenario).
  // Derived at construction from qobject_cast<IntervalModel*>(parent); never serialized.
  Id<Scenario::StateModel> m_parentStartStateId{};
  Id<Scenario::StateModel> m_parentEndStateId{};
};

// Free accessor helpers (mirrors ScenarioModel pattern for template compatibility)
inline auto& intervals(const SequenceModel& seq) { return seq.intervals; }
inline auto& events(const SequenceModel& seq) { return seq.events; }
inline auto& timeSyncs(const SequenceModel& seq) { return seq.timeSyncs; }
inline auto& states(const SequenceModel& seq) { return seq.states; }

} // namespace Sequence

// ElementTraits specialisations for SequenceModel
namespace Scenario
{
template <>
struct ElementTraits<Sequence::SequenceModel, IntervalModel>
{
  static const constexpr auto accessor
      = static_cast<const score::EntityMap<IntervalModel>& (*)(
          const Sequence::SequenceModel&)>(&Sequence::intervals);
};
template <>
struct ElementTraits<Sequence::SequenceModel, EventModel>
{
  static const constexpr auto accessor
      = static_cast<const score::EntityMap<EventModel>& (*)(
          const Sequence::SequenceModel&)>(&Sequence::events);
};
template <>
struct ElementTraits<Sequence::SequenceModel, TimeSyncModel>
{
  static const constexpr auto accessor
      = static_cast<const score::EntityMap<TimeSyncModel>& (*)(
          const Sequence::SequenceModel&)>(&Sequence::timeSyncs);
};
template <>
struct ElementTraits<Sequence::SequenceModel, StateModel>
{
  static const constexpr auto accessor
      = static_cast<const score::EntityMap<StateModel>& (*)(
          const Sequence::SequenceModel&)>(&Sequence::states);
};
} // namespace Scenario

DESCRIPTION_METADATA(SCORE_PLUGIN_SCENARIO_EXPORT, Sequence::SequenceModel, "Sequence")

Q_DECLARE_METATYPE(const Sequence::SequenceModel*)
Q_DECLARE_METATYPE(Sequence::SequenceModel*)
W_REGISTER_ARGTYPE(const Sequence::SequenceModel*)
W_REGISTER_ARGTYPE(Sequence::SequenceModel*)
