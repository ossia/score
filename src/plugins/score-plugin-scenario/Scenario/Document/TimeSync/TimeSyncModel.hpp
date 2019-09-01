#pragma once
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <State/Expression.hpp>
#include <Process/Dataflow/TimeSignature.hpp>

#include <score/model/Component.hpp>
#include <score/model/EntityImpl.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/Identifier.hpp>
#include <score/selection/Selectable.hpp>
#include <score/tools/Metadata.hpp>
#include <score/tools/std/Optional.hpp>

#include <QObject>

#include <score_plugin_scenario_export.h>
#include <verdigris>

#include <chrono>
class DataStream;
class JSONObject;

namespace Scenario
{
class EventModel;
class ScenarioInterface;

class SCORE_PLUGIN_SCENARIO_EXPORT TimeSyncModel final
    : public score::Entity<TimeSyncModel>
{
  W_OBJECT(TimeSyncModel)

  SCORE_SERIALIZE_FRIENDS

public:
  /** Properties of the class **/
  Selectable selection;

  /** The class **/
  TimeSyncModel(
      const Id<TimeSyncModel>& id,
      const TimeVal& date,
      QObject* parent);

  template <typename DeserializerVisitor>
  TimeSyncModel(DeserializerVisitor&& vis, QObject* parent)
      : Entity{vis, parent}
  {
    vis.writeTo(*this);
  }

  // Data of the TimeSync
  const TimeVal& date() const noexcept;
  void setDate(const TimeVal&);

  void addEvent(const Id<EventModel>&);
  bool removeEvent(const Id<EventModel>&);
  void clearEvents();

  using EventIdVec = ossia::small_vector<Id<EventModel>, 2>;
  const EventIdVec& events() const noexcept;
  void setEvents(const TimeSyncModel::EventIdVec& events);

  State::Expression expression() const noexcept { return m_expression; }
  void setExpression(const State::Expression& expression);

  bool active() const noexcept;
  void setActive(bool active);

  bool autotrigger() const noexcept;
  void setAutotrigger(bool t);

#if defined(SCORE_MUSICAL)
  optional<Control::time_signature> signature() const noexcept;
  void setSignature(optional<Control::time_signature> sig);

  double tempo() const noexcept;
  void setTempo(double);
#endif

public:
  void dateChanged(const TimeVal& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, dateChanged, arg_1)

  void newEvent(const Id<Scenario::EventModel>& eventId)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, newEvent, eventId)
  void eventRemoved(const Id<Scenario::EventModel>& eventId)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, eventRemoved, eventId)

  void triggerChanged(const State::Expression& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, triggerChanged, arg_1)
  void activeChanged() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, activeChanged)
  void autotriggerChanged(bool b) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, autotriggerChanged, b)

  void triggeredByGui() const
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, triggeredByGui)

#if defined(SCORE_MUSICAL)
  void signatureChanged(optional<Control::time_signature> sig)
    E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, signatureChanged, sig)

  void tempoChanged(double t)
  E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, tempoChanged, t)

  PROPERTY(double, tempo READ tempo WRITE setTempo NOTIFY tempoChanged)
  PROPERTY(optional<Control::time_signature>, signature READ signature WRITE setSignature NOTIFY signatureChanged)
#endif
private:
  TimeVal m_date{std::chrono::seconds{0}};
  State::Expression m_expression;

  EventIdVec m_events;
#if defined(SCORE_MUSICAL)
  double m_tempo{120.};
  optional<Control::time_signature> m_signature;
#endif
  bool m_active{false};
  bool m_autotrigger{false};
};
}

DEFAULT_MODEL_METADATA(Scenario::TimeSyncModel, "Sync")
TR_TEXT_METADATA(, Scenario::TimeSyncModel, PrettyName_k, "Sync")

Q_DECLARE_METATYPE(Id<Scenario::TimeSyncModel>)
W_REGISTER_ARGTYPE(Id<Scenario::TimeSyncModel>)

Q_DECLARE_METATYPE(optional<Control::time_signature>)
W_REGISTER_ARGTYPE(optional<Control::time_signature>)
