#pragma once
#include <Process/TimeValue.hpp>
#include <QObject>
#include <Scenario/Document/VerticalExtent.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <State/Expression.hpp>
#include <iscore/model/Entity.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/model/IdentifiedObject.hpp>
#include <iscore/tools/Metadata.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <QString>
#include <QVector>
#include <chrono>
#include <iscore/model/Component.hpp>
#include <iscore_plugin_scenario_export.h>
class DataStream;
class JSONObject;

namespace Scenario
{
class EventModel;
class ScenarioInterface;

class ISCORE_PLUGIN_SCENARIO_EXPORT TimeNodeModel final
    : public iscore::Entity<TimeNodeModel>
{
  Q_OBJECT

  ISCORE_SERIALIZE_FRIENDS

public:
  /** Properties of the class **/
  Selectable selection;

  /** The class **/
  TimeNodeModel(
      const Id<TimeNodeModel>& id,
      const VerticalExtent& extent,
      const TimeVal& date,
      QObject* parent);

  template <typename DeserializerVisitor>
  TimeNodeModel(DeserializerVisitor&& vis, QObject* parent)
      : Entity{vis, parent}
  {
    vis.writeTo(*this);
  }

  TimeNodeModel(
      const TimeNodeModel& source,
      const Id<TimeNodeModel>& id,
      QObject* parent);

  // Data of the TimeNode
  const VerticalExtent& extent() const;
  void setExtent(const VerticalExtent& extent);

  const TimeVal& date() const;
  void setDate(const TimeVal&);

  void addEvent(const Id<EventModel>&);
  bool removeEvent(const Id<EventModel>&);
  const QVector<Id<EventModel>>& events() const;
  void setEvents(const QVector<Id<EventModel>>& events);

  State::Expression expression() const { return m_expression; }
  void setExpression(const State::Expression& expression);

  bool active() const;
  void setActive(bool active);

  // Note : this is for API -> UI communication.
  // To trigger by hand we have the triggered() signal.
  ExecutionStatusProperty executionStatus; // TODO serialize me ?

signals:
  void extentChanged(const VerticalExtent&);
  void dateChanged(const TimeVal&);

  void newEvent(const Id<EventModel>& eventId);
  void eventRemoved(const Id<EventModel>& eventId);

  void triggerChanged(const State::Expression&);
  void activeChanged();

  void triggeredByGui() const;

private:
  VerticalExtent m_extent;
  TimeVal m_date{std::chrono::seconds{0}};

  State::Expression m_expression;
  bool m_active{false};

  // TODO small_vector<1>...
  QVector<Id<EventModel>> m_events;
};
}

DEFAULT_MODEL_METADATA(Scenario::TimeNodeModel, "Time Node")
TR_TEXT_METADATA(, Scenario::TimeNodeModel, PrettyName_k, "TimeNode")
