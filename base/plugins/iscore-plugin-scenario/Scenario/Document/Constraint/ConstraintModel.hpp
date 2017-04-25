#pragma once
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <QObject>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Constraint/ExecutionState.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
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
  ConstraintModel(DataStream::Deserializer& vis, QObject* parent);
  ConstraintModel(JSONObject::Deserializer& vis, QObject* parent);
  ConstraintModel(DataStream::Deserializer&& vis, QObject* parent);
  ConstraintModel(JSONObject::Deserializer&& vis, QObject* parent);

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

  // Full view properties:
  ZoomRatio zoom() const;
  void setZoom(const ZoomRatio& zoom);

  QRectF visibleRect() const;
  void setVisibleRect(const QRectF& value);

  void setSmallViewVisible(bool);
  bool smallViewVisible() const;

  const Rack& smallView() const { return m_smallView; }
  const Rack& fullView() const { return m_fullView; }

  void clearSmallView();
  void clearFullView();
  void replaceSmallView(const Rack& other);
  void replaceFullView(const Rack& other);

  void addSlot(Slot s);
  void addSlot(Slot s, SlotId pos);
  void removeSlot(SlotId pos);
  void swapSlots(int pos1, int pos2, Slot::RackView fullview);
  const Slot* findSlot(const SlotId& slot) const;
  const Slot& getSlot(const SlotId& slot) const;
  Slot& getSlot(const SlotId& slot);

  void setSlotHeight(const SlotId& slot, double height);

  void addLayer(const SlotId& slot, Id<Process::ProcessModel>);
  void removeLayer(const SlotId& slot, Id<Process::ProcessModel>);
  void putLayerToFront(const SlotId& slot, Id<Process::ProcessModel>);
  void putLayerToFront(const SlotId& slot, ossia::none_t);

signals:
  void heightPercentageChanged(double);

  void startDateChanged(const TimeVal&);

  void focusChanged(bool);
  void executionStateChanged(Scenario::ConstraintExecutionState);
  void executionStarted();
  void executionStopped();

  void smallViewVisibleChanged(bool fv);

  void rackChanged(Slot::RackView fv);
  void slotAdded(SlotId);
  void slotRemoved(SlotId);
  void slotResized(SlotId);
  void slotsSwapped(int slot1, int slot2, Slot::RackView fv);

  void layerAdded(SlotId, Id<Process::ProcessModel>);
  void layerRemoved(SlotId, Id<Process::ProcessModel>);
  void frontLayerChanged(SlotId, OptionalId<Process::ProcessModel>);

private:
  void on_addProcess(const Process::ProcessModel&);
  void on_removeProcess(const Process::ProcessModel&);
  void initConnections();

  // Model for the full view.
  // Note : it is also present in m_constraintViewModels.

  Rack m_smallView;
  Rack m_fullView;
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
