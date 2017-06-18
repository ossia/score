#pragma once
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <QObject>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Constraint/ExecutionState.hpp>
#include <Scenario/Document/Constraint/Slot.hpp>
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
  friend struct ConstraintSaveData;
  friend struct SlotPath;
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

  TimeVal midTime() const;
  void setMidTime(const TimeVal& value);

  void setSmallViewVisible(bool);
  bool smallViewVisible() const;

  const Rack& smallView() const { return m_smallView; }
  const FullRack& fullView() const { return m_fullView; }

  void clearSmallView();
  void clearFullView();
  void replaceSmallView(const Rack& other);
  void replaceFullView(const FullRack& other);

  // Adding and removing slots and layers is only for the small view
  void addSlot(Slot s);
  void addSlot(Slot s, int pos);
  void removeSlot(int pos);

  void addLayer(int slot, Id<Process::ProcessModel>);
  void removeLayer(int slot, Id<Process::ProcessModel>);

  void putLayerToFront(int slot, Id<Process::ProcessModel>);
  void putLayerToFront(int slot, ossia::none_t);

  void swapSlots(int pos1, int pos2, Slot::RackView fullview);

  double getSlotHeight(const SlotId& slot) const;
  void setSlotHeight(const SlotId& slot, double height);

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
  void heightFinishedChanging();

  void layerAdded(SlotId, Id<Process::ProcessModel>);
  void layerRemoved(SlotId, Id<Process::ProcessModel>);
  void frontLayerChanged(int, OptionalId<Process::ProcessModel>);

private:
  void on_addProcess(const Process::ProcessModel&);
  void on_removingProcess(const Process::ProcessModel&);
  void initConnections();

  const Slot* findSmallViewSlot(int slot) const;
  const Slot& getSmallViewSlot(int slot) const;
  Slot& getSmallViewSlot(int slot);

  const FullSlot* findFullViewSlot(int slot) const;
  const FullSlot& getFullViewSlot(int slot) const;
  FullSlot& getFullViewSlot(int slot);

  // Model for the full view.
  // Note : it is also present in m_constraintViewModels.

  Rack m_smallView;
  FullRack m_fullView;

  Id<StateModel> m_startState;
  Id<StateModel> m_endState;

  TimeVal m_startDate;
  double m_heightPercentage{0.5};

  ZoomRatio m_zoom{-1};
  TimeVal m_center{};
  ConstraintExecutionState m_executionState{};
  bool m_smallViewShown{};

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
