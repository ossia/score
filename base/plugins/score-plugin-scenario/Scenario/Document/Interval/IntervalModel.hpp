#pragma once
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <QObject>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/ExecutionState.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Document/ModelConsistency.hpp>
#include <score/model/Entity.hpp>
#include <score/selection/Selectable.hpp>
#include <score/tools/Metadata.hpp>
#include <score/tools/std/Optional.hpp>
#include <nano_signal_slot.hpp>

#include <QPointer>
#include <QString>
#include <QVector>

#include <Process/Dataflow/Port.hpp>
#include <score/model/Component.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/Identifier.hpp>

class DataStream;
class JSONObject;

namespace Scenario
{
class StateModel;

class SCORE_PLUGIN_SCENARIO_EXPORT IntervalModel final
    : public score::Entity<IntervalModel>,
      public Nano::Observer
{
  Q_OBJECT

  SCORE_SERIALIZE_FRIENDS
  friend struct IntervalSaveData;
  friend struct SlotPath;
  Q_PROPERTY(double heightPercentage READ heightPercentage WRITE setHeightPercentage NOTIFY heightPercentageChanged)

public:
  std::unique_ptr<Process::Inlet> inlet;
  std::unique_ptr<Process::Outlet> outlet;

  /** Properties of the class **/
  score::EntityMap<Process::ProcessModel> processes;

  Selectable selection;
  ModelConsistency consistency{nullptr};
  IntervalDurations duration{*this};

  /** The class **/
  IntervalModel(
      const Id<IntervalModel>&,
      double yPos,
      QObject* parent);

  ~IntervalModel();

  // Serialization
  IntervalModel(DataStream::Deserializer& vis, QObject* parent);
  IntervalModel(JSONObject::Deserializer& vis, QObject* parent);
  IntervalModel(DataStream::Deserializer&& vis, QObject* parent);
  IntervalModel(JSONObject::Deserializer&& vis, QObject* parent);

  const Id<StateModel>& startState() const;
  void setStartState(const Id<StateModel>& eventId);

  const Id<StateModel>& endState() const;
  void setEndState(const Id<StateModel>& endState);

  const TimeVal& date() const;
  void setStartDate(const TimeVal& start);
  void translate(const TimeVal& deltaTime);

  double heightPercentage() const;

  void startExecution();
  void stopExecution();
  // Resets the execution display recursively
  void reset();

  void setHeightPercentage(double arg);
  void setExecutionState(IntervalExecutionState);

  IntervalExecutionState executionState() const
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

  const Slot* findSmallViewSlot(int slot) const;
  const Slot& getSmallViewSlot(int slot) const;
  Slot& getSmallViewSlot(int slot);

  const FullSlot* findFullViewSlot(int slot) const;
  const FullSlot& getFullViewSlot(int slot) const;
  FullSlot& getFullViewSlot(int slot);
Q_SIGNALS:
  void heightPercentageChanged(double);

  void dateChanged(const TimeVal&);

  void focusChanged(bool);
  void executionStateChanged(Scenario::IntervalExecutionState);
  void executionStarted();
  void executionStopped();

  void smallViewVisibleChanged(bool fv);

  void rackChanged(Scenario::Slot::RackView fv);
  void slotAdded(Scenario::SlotId);
  void slotRemoved(Scenario::SlotId);
  void slotResized(Scenario::SlotId);
  void slotsSwapped(int slot1, int slot2, Slot::RackView fv);
  void heightFinishedChanging();

  void layerAdded(Scenario::SlotId, Id<Process::ProcessModel>);
  void layerRemoved(Scenario::SlotId, Id<Process::ProcessModel>);
  void frontLayerChanged(int, OptionalId<Process::ProcessModel>);

private:
  void on_addProcess(const Process::ProcessModel&);
  void on_removingProcess(const Process::ProcessModel&);
  void initConnections();

  // Model for the full view.
  // Note : it is also present in m_intervalViewModels.

  Rack m_smallView;
  FullRack m_fullView;

  Id<StateModel> m_startState;
  Id<StateModel> m_endState;

  TimeVal m_date;
  double m_heightPercentage{0.5};

  ZoomRatio m_zoom{-1};
  TimeVal m_center{};
  IntervalExecutionState m_executionState{};
  bool m_smallViewShown{};


};


SCORE_PLUGIN_SCENARIO_EXPORT
bool isInFullView(const Scenario::IntervalModel& cstr);
SCORE_PLUGIN_SCENARIO_EXPORT
bool isInFullView(const Process::ProcessModel& cstr);

}

DEFAULT_MODEL_METADATA(Scenario::IntervalModel, "Interval")

Q_DECLARE_METATYPE(Id<Scenario::IntervalModel>)
Q_DECLARE_METATYPE(Path<Scenario::IntervalModel>)
Q_DECLARE_METATYPE(QPointer<const Scenario::IntervalModel>)
TR_TEXT_METADATA(, Scenario::IntervalModel, PrettyName_k, "Interval")
