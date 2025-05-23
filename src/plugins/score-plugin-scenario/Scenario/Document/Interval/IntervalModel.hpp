#pragma once
#include <Process/Dataflow/TimeSignature.hpp>
#include <Process/Instantiations.hpp>
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>

#include <Scenario/Document/Interval/ExecutionState.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Document/Interval/TimeSignatureMap.hpp>
#include <Scenario/Document/Metatypes.hpp>
#include <Scenario/Document/ModelConsistency.hpp>

#include <score/model/Component.hpp>
#include <score/model/EntityImpl.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/Identifier.hpp>
#include <score/selection/Selectable.hpp>
#include <score/serialization/MapSerialization.hpp>
#include <score/tools/Metadata.hpp>
#include <score/tools/std/Optional.hpp>

#include <QObject>
#include <QPointer>

#include <nano_signal_slot.hpp>

#include <verdigris>

class DataStream;
class JSONObject;
namespace Process
{
class AudioInlet;
class AudioOutlet;
}
namespace Curve
{
class Model;
}
namespace Scenario
{
class StateModel;
class TempoProcess;

class SCORE_PLUGIN_SCENARIO_EXPORT IntervalModel final
    : public score::Entity<IntervalModel>
    , public Nano::Observer
{
  W_OBJECT(IntervalModel)

  SCORE_SERIALIZE_FRIENDS
  friend struct IntervalSaveData;
  friend struct SlotPath;

public:
  std::unique_ptr<Process::AudioInlet> inlet;
  std::unique_ptr<Process::AudioOutlet> outlet;

  /** Properties of the class **/
  score::EntityMap<Process::ProcessModel, true> processes;

  Selectable selection{this};
  ModelConsistency consistency{nullptr};
  IntervalDurations duration{*this};

  /** The class **/
  IntervalModel(
      const Id<IntervalModel>&, double yPos, const score::DocumentContext& ctx,
      QObject* parent);

  ~IntervalModel();

  // Serialization
  IntervalModel(
      DataStream::Deserializer& vis, const score::DocumentContext& ctx, QObject* parent);
  IntervalModel(
      JSONObject::Deserializer& vis, const score::DocumentContext& ctx, QObject* parent);
  IntervalModel(
      DataStream::Deserializer&& vis, const score::DocumentContext& ctx,
      QObject* parent);
  IntervalModel(
      JSONObject::Deserializer&& vis, const score::DocumentContext& ctx,
      QObject* parent);

  const score::DocumentContext& context() const noexcept { return m_context; }
  const Id<StateModel>& startState() const noexcept;
  void setStartState(const Id<StateModel>& eventId);

  const Id<StateModel>& endState() const noexcept;
  void setEndState(const Id<StateModel>& endState);

  const TimeVal& date() const noexcept;
  void setStartDate(const TimeVal& start);
  void translate(const TimeVal& deltaTime);

  double heightPercentage() const noexcept;

  void startExecution();
  void stopExecution();
  // Resets the execution display recursively
  void reset();

  void setHeightPercentage(double arg);
  void setExecutionState(IntervalExecutionState);

  IntervalExecutionState executionState() const;

  // Mode
  enum ViewMode : bool
  {
    Temporal = 0,
    Nodal = 1
  };
  ViewMode viewMode() const noexcept;
  void setViewMode(ViewMode v);
  void viewModeChanged(ViewMode v)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, viewModeChanged, v)

  // Node view properties (both small and full view)
  QPointF nodalOffset() const noexcept;
  void setNodalOffset(QPointF offs);
  double nodalScale() const noexcept;
  void setNodalScale(double z);

  // Full view properties:
  ZoomRatio zoom() const noexcept;
  void setZoom(const ::ZoomRatio& zoom);

  TimeVal midTime() const noexcept;
  void setMidTime(const TimeVal& value);

  void setSmallViewVisible(bool);
  bool smallViewVisible() const noexcept;

  const Rack& smallView() const noexcept { return m_smallView; }
  const FullRack& fullView() const noexcept { return m_fullView; }

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
  void putLayerToFront(int slot, std::nullopt_t);

  void swapSlots(int pos1, int pos2, Slot::RackView fullview);

  double getSlotHeight(const SlotId& slot) const;
  void setSlotHeight(const SlotId& slot, double height);
  double getHeight() const noexcept;

  //! Must be called with a process which is a child of this interval.
  double getSlotHeightForProcess(const Id<Process::ProcessModel>& p) const;

  const Slot* findSmallViewSlot(int slot) const;
  const Slot& getSmallViewSlot(int slot) const;
  Slot& getSmallViewSlot(int slot);

  const FullSlot* findFullViewSlot(int slot) const;
  const FullSlot& getFullViewSlot(int slot) const;
  FullSlot& getFullViewSlot(int slot);

  bool muted() const noexcept { return m_muted; }
  void setMuted(bool m);

  bool graphal() const noexcept { return m_graphal; }
  void setGraphal(bool m);

  bool executing() const noexcept { return m_executing; }
  void setExecuting(bool m);

  // Tempo stuff
  bool hasTimeSignature() const noexcept { return m_hasSignature; }
  void setHasTimeSignature(bool b);

  TimeVal contentDuration() const noexcept;

  TempoProcess* tempoCurve() const noexcept;

  void ancestorStartDateChanged();
  void ancestorTempoChanged();

  void addSignature(TimeVal t, ossia::time_signature sig);
  void removeSignature(TimeVal t);
  void setTimeSignatureMap(const TimeSignatureMap& map);
  const TimeSignatureMap& timeSignatureMap() const noexcept;

  ossia::musical_sync quantizationRate() const noexcept;
  void setQuantizationRate(ossia::musical_sync b);

  void setStartMarker(TimeVal t);
  TimeVal startMarker() const noexcept;

  void hasTimeSignatureChanged(bool arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, hasTimeSignatureChanged, arg_1)
  void timeSignaturesChanged(const TimeSignatureMap& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, timeSignaturesChanged, arg_1)
  void quantizationRateChanged(ossia::musical_sync arg)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, quantizationRateChanged, arg)

  void requestHeightChange(double y)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, requestHeightChange, y)
  void heightPercentageChanged(double arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, heightPercentageChanged, arg_1)

  void dateChanged(const TimeVal& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, dateChanged, arg_1)
  void startMarkerChanged(const TimeVal& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, startMarkerChanged, arg_1)

  void focusChanged(bool arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, focusChanged, arg_1)
  void executionStateChanged(Scenario::IntervalExecutionState arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, executionStateChanged, arg_1)

  void executionEvent(Scenario::IntervalExecutionEvent ev)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, executionEvent, ev)

  void smallViewVisibleChanged(bool fv)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, smallViewVisibleChanged, fv)

  void rackChanged(Scenario::Slot::RackView fv)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, rackChanged, fv)
  void slotAdded(Scenario::SlotId arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, slotAdded, arg_1)
  void slotRemoved(Scenario::SlotId arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, slotRemoved, arg_1)
  void slotResized(Scenario::SlotId arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, slotResized, arg_1)
  void slotsSwapped(int slot1, int slot2, Slot::RackView fv)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, slotsSwapped, slot1, slot2, fv)
  void heightFinishedChanging()
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, heightFinishedChanging)

  void layerAdded(Scenario::SlotId arg_1, Id<Process::ProcessModel> arg_2)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, layerAdded, arg_1, arg_2)
  void layerRemoved(Scenario::SlotId arg_1, Id<Process::ProcessModel> arg_2)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, layerRemoved, arg_1, arg_2)
  void frontLayerChanged(int arg_1, OptionalId<Process::ProcessModel> arg_2)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, frontLayerChanged, arg_1, arg_2)

  void mutedChanged(bool arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, mutedChanged, arg_1)
  void executingChanged(bool arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, executingChanged, arg_1)

  void busChanged(bool arg_1) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, busChanged, arg_1)

  void graphalChanged(bool arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, graphalChanged, arg_1)

  void nodalOffsetChanged(QPointF arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, nodalOffsetChanged, arg_1)
  void nodalScaleChanged(double arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, nodalScaleChanged, arg_1)

  PROPERTY(bool, muted READ muted WRITE setMuted NOTIFY mutedChanged)
  PROPERTY(bool, graphal READ graphal WRITE setGraphal NOTIFY graphalChanged)
  PROPERTY(
      double, heightPercentage READ heightPercentage WRITE setHeightPercentage NOTIFY
                  heightPercentageChanged)

  PROPERTY(
      bool, timeSignature READ hasTimeSignature WRITE setHasTimeSignature NOTIFY
                hasTimeSignatureChanged)

  PROPERTY(
      ossia::musical_sync, quantizationRate READ quantizationRate WRITE
                               setQuantizationRate NOTIFY quantizationRateChanged)
private:
  void on_addProcess(Process::ProcessModel&);
  void on_removingProcess(const Process::ProcessModel&);
  void initConnections();

  // Model for the full view.
  // Note : it is also present in m_intervalViewModels.
  const score::DocumentContext& m_context;

  Rack m_smallView;
  FullRack m_fullView;

  TimeSignatureMap m_signatures;

  Id<StateModel> m_startState;
  Id<StateModel> m_endState;

  TimeVal m_startMarker;

  TimeVal m_date;
  double m_heightPercentage{0.5};

  double m_nodalFullViewSlotHeight{100};
  double m_quantRate{-1.0};

  ::ZoomRatio m_zoom{-1};
  TimeVal m_center{};
  QPointF m_nodalOffset{};
  double m_nodalScale{1.0};
  IntervalExecutionState m_executionState : 2;
  ViewMode m_viewMode : 1;
  bool m_smallViewShown : 1;
  bool m_muted : 1;
  bool m_executing : 1;

  bool m_hasSignature : 1;
  bool m_graphal : 1;
};

SCORE_PLUGIN_SCENARIO_EXPORT
bool isInFullView(const Scenario::IntervalModel& cstr) noexcept;
SCORE_PLUGIN_SCENARIO_EXPORT
bool isInFullView(const Process::ProcessModel& cstr) noexcept;

SCORE_PLUGIN_SCENARIO_EXPORT
bool isBus(
    const Scenario::IntervalModel& model, const score::DocumentContext& ctx) noexcept;

SCORE_PLUGIN_SCENARIO_EXPORT
QPointF newProcessPosition(const Scenario::IntervalModel& model) noexcept;

struct ParentTimeInfo
{
  const IntervalModel* parent;
  const IntervalModel* lastFound;
  TimeVal delta;
};

SCORE_PLUGIN_SCENARIO_EXPORT
IntervalModel* closestParentInterval(const QObject*) noexcept;

SCORE_PLUGIN_SCENARIO_EXPORT
ParentTimeInfo closestParentWithMusicalMetrics(const IntervalModel*);
SCORE_PLUGIN_SCENARIO_EXPORT
ParentTimeInfo closestParentWithTempo(const IntervalModel*);
SCORE_PLUGIN_SCENARIO_EXPORT
ParentTimeInfo closestParentWithQuantification(const IntervalModel* self);

SCORE_PLUGIN_SCENARIO_EXPORT
TimeVal timeDelta(const IntervalModel* child, const IntervalModel* parent);
}

DEFAULT_MODEL_METADATA(Scenario::IntervalModel, "Interval")

Q_DECLARE_METATYPE(Scenario::IntervalModel::ViewMode)
W_REGISTER_ARGTYPE(Scenario::IntervalModel::ViewMode)
Q_DECLARE_METATYPE(QPointer<Scenario::IntervalModel>)
W_REGISTER_ARGTYPE(QPointer<Scenario::IntervalModel>)

TR_TEXT_METADATA(, Scenario::IntervalModel, PrettyName_k, "Interval")

Q_DECLARE_METATYPE(Scenario::IntervalModel*)
W_REGISTER_ARGTYPE(Scenario::IntervalModel*)
Q_DECLARE_METATYPE(const Scenario::IntervalModel*)
W_REGISTER_ARGTYPE(const Scenario::IntervalModel*)
