#pragma once
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <QObject>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Constraint/ExecutionState.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/ModelConsistency.hpp>
#include <iscore/model/Entity.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/tools/Metadata.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <nano_signal_slot.hpp>

#include <QString>
#include <QVector>

#include <iscore/model/Component.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/IdentifiedObject.hpp>
#include <iscore/model/Identifier.hpp>
class DataStream;
class JSONObject;

namespace Scenario
{
class StateModel;

class ISCORE_PLUGIN_SCENARIO_EXPORT ConstraintModel final
    : public iscore::Entity<ConstraintModel>,
      public Nano::Observer
{
  Q_OBJECT

  ISCORE_SERIALIZE_FRIENDS
  friend class ConstraintSaveData;
  // TODO must go in view model
  Q_PROPERTY(double heightPercentage READ heightPercentage WRITE
                 setHeightPercentage NOTIFY heightPercentageChanged)

public:
  /** Properties of the class **/
  iscore::EntityMap<Process::ProcessModel> processes;

  RackModel& smallViewRack() const { return *m_smallViewRack; }
  RackModel& fullViewRack() const { return *m_fullViewRack; }
  iscore::EntityMap<RackModel> racks;

  Selectable selection;
  ModelConsistency consistency{nullptr};
  ConstraintDurations duration{*this};

  /** The class **/
  ConstraintModel(
      const Id<ConstraintModel>&,
      double yPos,
      QObject* parent);

  ~ConstraintModel();

  // Copy
  ConstraintModel(
      const ConstraintModel& source,
      const Id<ConstraintModel>& id,
      QObject* parent);

  // Serialization
  template <typename Deserializer>
  ConstraintModel(Deserializer&& vis, QObject* parent) : Entity{vis, parent}
  {
    vis.writeTo(*this);
  }

  const Id<StateModel>& startState() const;
  void setStartState(const Id<StateModel>& eventId);

  const Id<StateModel>& endState() const;
  void setEndState(const Id<StateModel>& endState);

  const TimeVal& startDate() const;
  void setStartDate(const TimeVal& start);
  void translate(const TimeVal& deltaTime);

  double heightPercentage() const;

  void startExecution();
  void stopExecution();
  // Resets the execution display recursively
  void reset();

  void setHeightPercentage(double arg);
  void setExecutionState(ConstraintExecutionState);
  ConstraintExecutionState executionState() const
  {
    return m_executionState;
  }

  static Id<RackModel> smallViewRackId();
  static Id<RackModel> fullViewRackId();

  // Full view properties:


  ZoomRatio zoom() const;
  void setZoom(const ZoomRatio& zoom);

  QRectF visibleRect() const;
  void setVisibleRect(const QRectF& value);

  void setSmallViewVisible(bool);
  bool smallViewVisible() const;
signals:
  void heightPercentageChanged(double);

  void startDateChanged(const TimeVal&);

  void focusChanged(bool);
  void executionStateChanged(Scenario::ConstraintExecutionState);
  void executionStarted();
  void executionStopped();

  void smallViewVisibleChanged(bool);

private:
  void on_rackAdded(const RackModel& rack);

  // Model for the full view.
  // Note : it is also present in m_constraintViewModels.

  RackModel* m_smallViewRack{};
  RackModel* m_fullViewRack{};

  Id<StateModel> m_startState;
  Id<StateModel> m_endState;

  TimeVal m_startDate; // origin

  double m_heightPercentage{0.5};

  ZoomRatio m_zoom{-1};
  QRectF m_center{};
  bool m_smallViewShown{};
  ConstraintExecutionState m_executionState{};
};

ISCORE_PLUGIN_SCENARIO_EXPORT
bool isInFullView(const Scenario::ConstraintModel& cstr);
ISCORE_PLUGIN_SCENARIO_EXPORT
bool isInFullView(const Process::ProcessModel& cstr);

}

DEFAULT_MODEL_METADATA(Scenario::ConstraintModel, "Constraint")

Q_DECLARE_METATYPE(Id<Scenario::ConstraintModel>)
Q_DECLARE_METATYPE(Path<Scenario::ConstraintModel>)
TR_TEXT_METADATA(, Scenario::ConstraintModel, PrettyName_k, "Constraint")
