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
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Process/ScenarioProcessMetadata.hpp>
#include <boost/iterator/indirect_iterator.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/IdentifiedObjectMap.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/selection/Selection.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/model/Identifier.hpp>

class DataStream;
class JSONObject;
class QEvent;

namespace Scenario
{
class ScenarioFactory;
/**
 * @brief The core hierarchical and temporal process of i-score
 */
class ISCORE_PLUGIN_SCENARIO_EXPORT ProcessModel final
    : public Process::ProcessModel,
      public ScenarioInterface
{
  Q_OBJECT

  ISCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Scenario::ProcessModel)
  friend class ScenarioFactory;
  friend class ScenarioTemporalLayerFactory;

public:
  ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);
  ~ProcessModel();

  //// ScenarioModel specifics ////

  // Accessors
  ElementContainer<ConstraintModel> getConstraints() const final override
  {
    auto& map = constraints.map().get();
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

  ElementContainer<TimeNodeModel> getTimeNodes() const final override
  {
    auto& map = timeNodes.map().get();
    return {map.begin(), map.end()};
  }

  ConstraintModel*
  findConstraint(const Id<ConstraintModel>& id) const final override
  {
    return ossia::ptr_find(constraints, id);
  }
  EventModel* findEvent(const Id<EventModel>& id) const final override
  {
    return ossia::ptr_find(events, id);
  }
  TimeNodeModel* findTimeNode(const Id<TimeNodeModel>& id) const final override
  {
    return ossia::ptr_find(timeNodes, id);
  }
  StateModel* findState(const Id<StateModel>& id) const final override
  {
    return ossia::ptr_find(states, id);
  }

  ConstraintModel&
  constraint(const Id<ConstraintModel>& constraintId) const final override
  {
    return constraints.at(constraintId);
  }
  EventModel& event(const Id<EventModel>& eventId) const final override
  {
    return events.at(eventId);
  }
  TimeNodeModel&
  timeNode(const Id<TimeNodeModel>& timeNodeId) const final override
  {
    return timeNodes.at(timeNodeId);
  }
  StateModel& state(const Id<StateModel>& stId) const final override
  {
    return states.at(stId);
  }
  CommentBlockModel& comment(const Id<CommentBlockModel>& cmtId) const
  {
    return comments.at(cmtId);
  }

  TimeNodeModel& startTimeNode() const final override
  {
    return timeNodes.at(m_startTimeNodeId);
  }

  EventModel& startEvent() const
  {
    return events.at(m_startEventId);
  }

  iscore::EntityMap<ConstraintModel> constraints;
  iscore::EntityMap<EventModel> events;
  iscore::EntityMap<TimeNodeModel> timeNodes;
  iscore::EntityMap<StateModel> states;
  iscore::EntityMap<CommentBlockModel> comments;

signals:
  void stateMoved(const StateModel&);
  void eventMoved(const EventModel&);
  void constraintMoved(const ConstraintModel&);
  void commentMoved(const CommentBlockModel&);

  void locked();
  void unlocked();

public slots:
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

  template <typename Impl>
  ProcessModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  // To prevent warnings in Clang
  bool event(QEvent* e) override
  {
    return QObject::event(e);
  }

private:
  bool contentHasDuration() const override;
  TimeVal contentDuration() const override;

  template <typename Fun>
  void apply(Fun fun) const
  {
    fun(&ProcessModel::constraints);
    fun(&ProcessModel::states);
    fun(&ProcessModel::events);
    fun(&ProcessModel::timeNodes);
    fun(&ProcessModel::comments);
  }
  ProcessModel(
      const Scenario::ProcessModel& source,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  Id<TimeNodeModel> m_startTimeNodeId{};

  Id<EventModel> m_startEventId{};

  Id<StateModel> m_startStateId{};
  // By default, creation in the void will make a constraint
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
ISCORE_PLUGIN_SCENARIO_EXPORT const QVector<Id<ConstraintModel>>
constraintsBeforeTimeNode(
    const Scenario::ProcessModel&, const Id<TimeNodeModel>& timeNodeId);

const StateModel*
furthestSelectedState(const Scenario::ProcessModel& scenario);
const StateModel* furthestSelectedStateWithoutFollowingConstraint(
    const Scenario::ProcessModel& scenario);

inline auto& constraints(const Scenario::ProcessModel& scenar)
{
  return scenar.constraints;
}
inline auto& events(const Scenario::ProcessModel& scenar)
{
  return scenar.events;
}
inline auto& timeNodes(const Scenario::ProcessModel& scenar)
{
  return scenar.timeNodes;
}
inline auto& states(const Scenario::ProcessModel& scenar)
{
  return scenar.states;
}

template <>
struct ElementTraits<Scenario::ProcessModel, ConstraintModel>
{
  static const constexpr auto accessor
      = static_cast<const iscore::EntityMap<ConstraintModel>& (*)(const Scenario::
                                                              ProcessModel&)>(
          &constraints);
};
template <>
struct ElementTraits<Scenario::ProcessModel, EventModel>
{
  static const constexpr auto accessor
      = static_cast<const iscore::EntityMap<EventModel>& (*)(const Scenario::
                                                         ProcessModel&)>(
          &events);
};
template <>
struct ElementTraits<Scenario::ProcessModel, TimeNodeModel>
{
  static const constexpr auto accessor
      = static_cast<const iscore::EntityMap<TimeNodeModel>& (*)(const Scenario::
                                                            ProcessModel&)>(
          &timeNodes);
};
template <>
struct ElementTraits<Scenario::ProcessModel, StateModel>
{
  static const constexpr auto accessor
      = static_cast<const iscore::EntityMap<StateModel>& (*)(const Scenario::
                                                         ProcessModel&)>(
          &states);
};
}
DESCRIPTION_METADATA(
    ISCORE_PLUGIN_SCENARIO_EXPORT, Scenario::ProcessModel, "Scenario")
