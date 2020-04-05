#pragma once
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <State/Expression.hpp>
#include <Scenario/Document/Metatypes.hpp>

#include <score/model/Component.hpp>
#include <score/model/EntityImpl.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/Identifier.hpp>
#include <score/selection/Selectable.hpp>
#include <score/tools/Metadata.hpp>
#include <score/tools/std/Optional.hpp>


#include <score_plugin_scenario_export.h>
#include <verdigris>
class DataStream;
class JSONObject;
class QObject;

namespace Process
{
struct Style;
}

namespace Scenario
{
class StateModel;
class ScenarioInterface;
class TimeSyncModel;

class SCORE_PLUGIN_SCENARIO_EXPORT EventModel final
    : public score::Entity<EventModel>
{
  W_OBJECT(EventModel)

  SCORE_SERIALIZE_FRIENDS

public:
  /** Public properties of the class **/
  Selectable selection;

  /** The class **/
  EventModel(
      const Id<EventModel>&,
      const Id<TimeSyncModel>& timesync,
      const TimeVal& date,
      QObject* parent);

  template <typename DeserializerVisitor>
  EventModel(DeserializerVisitor&& vis, QObject* parent) : Entity{vis, parent}
  {
    vis.writeTo(*this);
  }

  // Timenode
  void changeTimeSync(const Id<TimeSyncModel>& elt);
  const Id<TimeSyncModel>& timeSync() const noexcept { return m_timeSync; }

  // States
  void addState(const Id<StateModel>& ds);
  void removeState(const Id<StateModel>& ds);
  void clearStates();
  using StateIdVec = ossia::small_vector<Id<StateModel>, 2>;
  const StateIdVec& states() const noexcept;

  // Other properties
  const State::Expression& condition() const noexcept;
  OffsetBehavior offsetBehavior() const noexcept;
  const TimeVal& date() const noexcept;
  void translate(const TimeVal& deltaTime);
  ExecutionStatus status() const noexcept;

  void setCondition(const State::Expression& arg);
  void setDate(const TimeVal& date);
  void setStatus(
      Scenario::ExecutionStatus status,
      const Scenario::ScenarioInterface&);
  void setOffsetBehavior(Scenario::OffsetBehavior);

  const QBrush& color(const Process::Style&) const noexcept;

public:
  void dateChanged(const TimeVal& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, dateChanged, arg_1)
  void conditionChanged(const State::Expression& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, conditionChanged, arg_1)
  void statesChanged() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, statesChanged)
  void statusChanged(Scenario::ExecutionStatus status)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, statusChanged, status)
  void offsetBehaviorChanged(OffsetBehavior arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, offsetBehaviorChanged, arg_1)
  void timeSyncChanged(Id<Scenario::TimeSyncModel> oldt, Id<Scenario::TimeSyncModel> newt)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, timeSyncChanged, oldt, newt)

private:
  Id<TimeSyncModel> m_timeSync;
  StateIdVec m_states;
  State::Expression m_condition;
  TimeVal m_date{TimeVal::zero()};

  ExecutionStatusProperty m_status{};
  OffsetBehavior m_offset{};

  W_PROPERTY(
      Scenario::OffsetBehavior,
      offsetBehavior READ offsetBehavior WRITE setOffsetBehavior NOTIFY
          offsetBehaviorChanged,
      W_Final)
};
}

DEFAULT_MODEL_METADATA(Scenario::EventModel, "Event")
TR_TEXT_METADATA(, Scenario::EventModel, PrettyName_k, "Event")
