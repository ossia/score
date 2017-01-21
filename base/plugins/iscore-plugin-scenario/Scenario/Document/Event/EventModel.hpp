#pragma once
#include <Process/TimeValue.hpp>
#include <QString>
#include <QVector>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <State/Expression.hpp>
#include <iscore/model/Component.hpp>
#include <iscore/model/Entity.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/model/IdentifiedObject.hpp>
#include <iscore/tools/Metadata.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore_plugin_scenario_export.h>
class DataStream;
class JSONObject;
class QObject;

namespace Scenario
{
class StateModel;
class ScenarioInterface;
class TimeNodeModel;

class ISCORE_PLUGIN_SCENARIO_EXPORT EventModel final
    : public iscore::Entity<EventModel>
{
  Q_OBJECT

  ISCORE_SERIALIZE_FRIENDS

  Q_PROPERTY(Scenario::OffsetBehavior offsetBehavior READ offsetBehavior WRITE
                 setOffsetBehavior NOTIFY offsetBehaviorChanged FINAL)

public:
  /** Public properties of the class **/
  Selectable selection;

  /** The class **/
  EventModel(
      const Id<EventModel>&,
      const Id<TimeNodeModel>& timenode,
      const VerticalExtent& extent,
      const TimeVal& date,
      QObject* parent);

  // Copy
  EventModel(const EventModel& source, const Id<EventModel>&, QObject* parent);

  template <typename DeserializerVisitor>
  EventModel(DeserializerVisitor&& vis, QObject* parent) : Entity{vis, parent}
  {
    vis.writeTo(*this);
  }

  // Timenode
  void changeTimeNode(const Id<TimeNodeModel>& elt)
  {
    m_timeNode = elt;
  }
  const auto& timeNode() const
  {
    return m_timeNode;
  }

  // States
  void addState(const Id<StateModel>& ds);
  void removeState(const Id<StateModel>& ds);
  const QVector<Id<StateModel>>& states() const;

  // Other properties
  const State::Expression& condition() const;
  OffsetBehavior offsetBehavior() const;

  VerticalExtent extent() const;

  const TimeVal& date() const;
  void translate(const TimeVal& deltaTime);

  ExecutionStatus status() const;
  void reset();

  void setCondition(const State::Expression& arg);

  void setExtent(const VerticalExtent& extent);
  void setDate(const TimeVal& date);

  void setStatus(ExecutionStatus status);
  void setOffsetBehavior(OffsetBehavior);

signals:
  void extentChanged(const VerticalExtent&);
  void dateChanged(const TimeVal&);

  void conditionChanged(const State::Expression&);

  void statesChanged();

  void statusChanged(Scenario::ExecutionStatus status);

  void offsetBehaviorChanged(OffsetBehavior);

private:
  Id<TimeNodeModel> m_timeNode;

  QVector<Id<StateModel>> m_states;

  State::Expression m_condition;

  VerticalExtent m_extent;
  TimeVal m_date{TimeVal::zero()};

  ExecutionStatusProperty m_status{};
  OffsetBehavior m_offset{};
};
}

DEFAULT_MODEL_METADATA(Scenario::EventModel, "Event")
TR_TEXT_METADATA(, Scenario::EventModel, PrettyName_k, "Event")
