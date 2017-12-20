#pragma once
#include <Process/TimeValue.hpp>
#include <QString>
#include <QVector>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <State/Expression.hpp>
#include <score/model/Component.hpp>
#include <score/model/Entity.hpp>
#include <score/selection/Selectable.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/tools/Metadata.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/std/Optional.hpp>
#include <score_plugin_scenario_export.h>
class DataStream;
class JSONObject;
class QObject;

namespace Scenario
{
class StateModel;
class ScenarioInterface;
class TimeSyncModel;

class SCORE_PLUGIN_SCENARIO_EXPORT EventModel final
    : public score::Entity<EventModel>
{
  Q_OBJECT

  SCORE_SERIALIZE_FRIENDS

  Q_PROPERTY(Scenario::OffsetBehavior offsetBehavior READ offsetBehavior WRITE
                 setOffsetBehavior NOTIFY offsetBehaviorChanged FINAL)

public:
  /** Public properties of the class **/
  Selectable selection;

  /** The class **/
  EventModel(
      const Id<EventModel>&,
      const Id<TimeSyncModel>& timesync,
      const VerticalExtent& extent,
      const TimeVal& date,
      QObject* parent);

  template <typename DeserializerVisitor>
  EventModel(DeserializerVisitor&& vis, QObject* parent) : Entity{vis, parent}
  {
    vis.writeTo(*this);
  }

  // Timenode
  void changeTimeSync(const Id<TimeSyncModel>& elt)
  {
    m_timeSync = elt;
  }
  const auto& timeSync() const
  {
    return m_timeSync;
  }

  // States
  void addState(const Id<StateModel>& ds);
  void removeState(const Id<StateModel>& ds);
  void clearStates();
  const QVector<Id<StateModel>>& states() const;

  // Other properties
  const State::Expression& condition() const;
  OffsetBehavior offsetBehavior() const;

  VerticalExtent extent() const;

  const TimeVal& date() const;
  void translate(const TimeVal& deltaTime);

  ExecutionStatus status() const;

  void setCondition(const State::Expression& arg);

  void setExtent(const VerticalExtent& extent);
  void setDate(const TimeVal& date);

  void setStatus(ExecutionStatus status, const Scenario::ScenarioInterface&);
  void setOffsetBehavior(OffsetBehavior);

signals:
  void extentChanged(const VerticalExtent&);
  void dateChanged(const TimeVal&);

  void conditionChanged(const State::Expression&);

  void statesChanged();

  void statusChanged(Scenario::ExecutionStatus status);

  void offsetBehaviorChanged(OffsetBehavior);

private:
  Id<TimeSyncModel> m_timeSync;

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
