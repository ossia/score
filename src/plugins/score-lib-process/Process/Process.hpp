#pragma once
#include <Process/Dataflow/PortForward.hpp>
#include <Process/CodeFormat.hpp>
#include <Process/ExpandMode.hpp>
#include <Process/Preset.hpp>
#include <Process/ProcessFlags.hpp>
#include <Process/TimeValue.hpp>

#include <Magnetism/MagneticInfo.hpp>

#include <score/model/Component.hpp>
#include <score/model/EntityImpl.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/plugins/SerializableInterface.hpp>
#include <score/selection/Selectable.hpp>
#include <score/selection/Selection.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score/tools/Metadata.hpp>

#include <ossia/detail/small_vector.hpp>

#include <QString>

#include <score_lib_process_export.h>
#include <smallfun.hpp>

#include <optional>
#include <vector>
#include <verdigris>

class ProcessStateDataInterface;
namespace ossia
{
class value;
}
namespace Process
{
class Port;
class Inlet;
class ControlInlet;
class Outlet;
class AudioInlet;
class AudioOutlet;
class MidiInlet;
class MidiOutlet;
class ProcessModelFactory;
class LayerFactory;
class ProcessModel;
class LayerFactory;
struct Inlets;
struct Outlets;
class CodeWriter;

/**
 * @brief The Process class
 *
 * Interface to implement to make a process.
 */
class SCORE_LIB_PROCESS_EXPORT ProcessModel
    : public score::Entity<ProcessModel>
    , public score::SerializableInterface<ProcessModel>
{
  W_OBJECT(ProcessModel)
  SCORE_SERIALIZE_FRIENDS
public:
  Selectable selection{this};
  ProcessModel(
      TimeVal duration, const Id<ProcessModel>& id, const QString& name,
      QObject* parent);

  ProcessModel(DataStream::Deserializer& vis, QObject* parent);
  ProcessModel(JSONObject::Deserializer& vis, QObject* parent);

  virtual ~ProcessModel();

  // If the process can have multiple distinct implementations
  // for which presets are incompatible, e.g. VST, Faust...
  // This allows to differentiate them
  virtual QString effect() const noexcept;

  // A user-friendly text to show to the users
  virtual QString prettyName() const noexcept;
  virtual QString prettyShortName() const noexcept = 0;
  virtual QString category() const noexcept = 0;
  virtual QStringList tags() const noexcept = 0;
  virtual ProcessFlags flags() const noexcept = 0;
  virtual std::unique_ptr<CodeWriter> codeWriter(CodeFormat) const noexcept;

  //// Features of a process
  /// Duration
  void setParentDuration(ExpandMode mode, const TimeVal& t) noexcept;

  virtual TimeVal contentDuration() const noexcept;

  void setDuration(const TimeVal& other) noexcept;
  const TimeVal& duration() const noexcept;

  /// Nodal things
  QPointF position() const noexcept;
  QSizeF size() const noexcept;

  void setPosition(const QPointF& v);
  void setSize(const QSizeF& v);

  void positionChanged(QPointF p) E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, positionChanged, p)
  void sizeChanged(QSizeF p) E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, sizeChanged, p)

  PROPERTY(
      QPointF, position W_READ position W_WRITE setPosition W_NOTIFY positionChanged)
  PROPERTY(QSizeF, size W_READ size W_WRITE setSize W_NOTIFY sizeChanged)

  /// States. The process has ownership.
  virtual ProcessStateDataInterface* startStateData() const noexcept;
  virtual ProcessStateDataInterface* endStateData() const noexcept;

  /// Selection
  virtual Selection selectableChildren() const noexcept;
  virtual Selection selectedChildren() const noexcept;
  virtual void setSelection(const Selection& s) const noexcept;

  double getSlotHeight() const noexcept;
  void setSlotHeight(double) noexcept;

  const Process::Inlets& inlets() const noexcept { return m_inlets; }
  const Process::Outlets& outlets() const noexcept { return m_outlets; }

  Process::Inlet* inlet(const Id<Process::Port>&) const noexcept;
  Process::Outlet* outlet(const Id<Process::Port>&) const noexcept;

  virtual void loadPreset(const Preset& preset);
  virtual Preset savePreset() const noexcept;
  virtual std::vector<Process::Preset> builtinPresets() const noexcept;

  // Called when an ancestor was moved in the timeline
  virtual void ancestorStartDateChanged();
  virtual void ancestorTempoChanged();

  virtual void forEachControl(
      smallfun::function<void(Process::ControlInlet&, const ossia::value&)>) const;

  // Magnetism
  virtual std::optional<Process::MagneticInfo>
  magneticPosition(const QObject* o, const TimeVal t) const noexcept;

  // Clip duration things
  bool loops() const noexcept { return m_loops; }
  void setLoops(bool b);
  void loopsChanged(bool b) E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, loopsChanged, b)
  PROPERTY(bool, loops W_READ loops W_WRITE setLoops W_NOTIFY loopsChanged)

  TimeVal startOffset() const noexcept { return m_startOffset; }
  void setStartOffset(TimeVal b);
  void startOffsetChanged(TimeVal b)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, startOffsetChanged, b)
  PROPERTY(
      TimeVal,
      startOffset W_READ startOffset W_WRITE setStartOffset W_NOTIFY startOffsetChanged)

  TimeVal loopDuration() const noexcept { return m_loopDuration; }
  void setLoopDuration(TimeVal b);
  void loopDurationChanged(TimeVal b)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, loopDurationChanged, b)
  PROPERTY(
      TimeVal, loopDuration W_READ loopDuration W_WRITE setLoopDuration W_NOTIFY
                   loopDurationChanged)

  virtual void setCreatingControls(bool ok);
  bool creatingControls() const noexcept;
  void creatingControlsChanged(bool b)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, creatingControlsChanged, b)
  PROPERTY(
      bool, creatingControls W_READ creatingControls W_WRITE setCreatingControls W_NOTIFY
                creatingControlsChanged)

  // FIXME ugh
  QWidget* externalUI{};

  /// Execution
  void startExecution() E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, startExecution)
  void stopExecution() E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, stopExecution)
  void resetExecution() E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, resetExecution)

  void durationChanged(const TimeVal& arg_1)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, durationChanged, arg_1)
  void useParentDurationChanged(bool arg_1)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, useParentDurationChanged, arg_1)

  void slotHeightChanged(double arg_1)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, slotHeightChanged, arg_1)
  void prettyNameChanged() E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, prettyNameChanged)

  void inletsChanged() E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, inletsChanged)
  void outletsChanged() E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, outletsChanged)

  void controlAdded(const Id<Process::Port>& arg_1)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, controlAdded, arg_1)
  void controlRemoved(const Process::Port& arg_1)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, controlRemoved, arg_1)
  void controlOutletAdded(const Id<Process::Port>& arg_1)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, controlOutletAdded, arg_1)
  void controlOutletRemoved(const Process::Port& arg_1)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, controlOutletRemoved, arg_1)

  void benchmark(double arg_1) E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, benchmark, arg_1)
  void externalUIVisible(bool v) const
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, externalUIVisible, v)

protected:
  // Used to scale the process.
  // This should be commutative :
  //   setDurationWithScale(2); setDurationWithScale(3);
  // yields the same result as :
  //   setDurationWithScale(3); setDurationWithScale(2);
  virtual void setDurationAndScale(const TimeVal& newDuration) noexcept;

  // Does nothing if newDuration < currentDuration
  virtual void setDurationAndGrow(const TimeVal& newDuration) noexcept;

  // Does nothing if newDuration > currentDuration
  virtual void setDurationAndShrink(const TimeVal& newDuration) noexcept;

  Process::Inlets m_inlets;
  Process::Outlets m_outlets;

private:
  TimeVal m_duration{};
  double m_slotHeight{}; //! Height in full view
  TimeVal m_startOffset{};
  TimeVal m_loopDuration{};
  QPointF m_position{};
  QSizeF m_size{};
  bool m_loops{};
};

SCORE_LIB_PROCESS_EXPORT
ProcessModel* parentProcess(QObject* obj) noexcept;
SCORE_LIB_PROCESS_EXPORT
const ProcessModel* parentProcess(const QObject* obj) noexcept;

SCORE_LIB_PROCESS_EXPORT
QString processLocalTreeAddress(const Process::ProcessModel& proc);
}
DEFAULT_MODEL_METADATA(Process::ProcessModel, "Process")

Q_DECLARE_METATYPE(Id<Process::ProcessModel>)
W_REGISTER_ARGTYPE(Id<Process::ProcessModel>)
W_REGISTER_ARGTYPE(OptionalId<Process::ProcessModel>)

Q_DECLARE_METATYPE(Process::ProcessModel*)
Q_DECLARE_METATYPE(const Process::ProcessModel*)
W_REGISTER_ARGTYPE(Process::ProcessModel*)
W_REGISTER_ARGTYPE(const Process::ProcessModel*)
W_REGISTER_ARGTYPE(QPointer<const Process::ProcessModel>)
W_REGISTER_ARGTYPE(QPointer<Process::ProcessModel>)

Q_DECLARE_METATYPE(std::shared_ptr<Process::Preset>)
W_REGISTER_ARGTYPE(std::shared_ptr<Process::Preset>)
