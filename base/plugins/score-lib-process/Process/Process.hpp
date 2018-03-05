#pragma once
#include <Process/ExpandMode.hpp>

#include <Process/TimeValue.hpp>
#include <QByteArray>
#include <QString>
#include <score/model/Entity.hpp>
#include <score/selection/Selection.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <vector>

#include <score/model/Component.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/plugins/customfactory/SerializableInterface.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score/tools/Metadata.hpp>
#include <score/selection/Selectable.hpp>
#include <score/model/Identifier.hpp>
#include <score_lib_process_export.h>
#include <ossia/detail/small_vector.hpp>

class ProcessStateDataInterface;

namespace Process
{
class Port;
class Inlet;
class Outlet;
class ProcessModelFactory;
class LayerFactory;
class ProcessModel;
class LayerFactory;
using Inlets = ossia::small_vector<Process::Inlet*, 4>;
using Outlets = ossia::small_vector<Process::Outlet*, 4>;

/**
 * @brief The Process class
 *
 * Interface to implement to make a process.
 */
class SCORE_LIB_PROCESS_EXPORT ProcessModel
    : public score::Entity<ProcessModel>
    , public score::SerializableInterface<ProcessModel>
{
  Q_OBJECT
  SCORE_SERIALIZE_FRIENDS
public:
  Selectable selection;
  ProcessModel(
      TimeVal duration,
      const Id<ProcessModel>& id,
      const QString& name,
      QObject* parent);

  ProcessModel(DataStream::Deserializer& vis, QObject* parent);
  ProcessModel(JSONObject::Deserializer& vis, QObject* parent);

  virtual ~ProcessModel();

  // A user-friendly text to show to the users
  virtual QString prettyName() const;
  virtual QString prettyShortName() const = 0;
  virtual QString category() const = 0;
  virtual QStringList tags() const = 0;

  //// Features of a process
  /// Duration
  void setParentDuration(ExpandMode mode, const TimeVal& t);

  virtual bool contentHasDuration() const;
  virtual TimeVal contentDuration() const;

  // TODO might not be useful... put in protected ?
  // Constructor needs it, too.
  void setDuration(const TimeVal& other);
  const TimeVal& duration() const;

  /// Execution
  virtual void startExecution();
  virtual void stopExecution();
  virtual void reset();

  /// States. The process has ownership.
  virtual ProcessStateDataInterface* startStateData() const;
  virtual ProcessStateDataInterface* endStateData() const;

  /// Selection
  virtual Selection selectableChildren() const;
  virtual Selection selectedChildren() const;
  virtual void setSelection(const Selection& s) const;

  double getSlotHeight() const;
  void setSlotHeight(double);

  const Process::Inlets& inlets() const { return m_inlets; }
  const Process::Outlets& outlets() const { return m_outlets; }

  Process::Inlet* inlet(const Id<Process::Port>&) const;
  Process::Outlet* outlet(const Id<Process::Port>&) const;

  QWidget* externalUI{};
Q_SIGNALS:
  // True if the execution is running.
  void execution(bool);
  void durationChanged(const TimeVal&);
  void useParentDurationChanged(bool);

  void slotHeightChanged(double);
  void prettyNameChanged();

  void inletsChanged();
  void outletsChanged();

  void controlAdded(const Id<Process::Port>&);
  void controlRemoved(const Process::Port&);

  void benchmark(double);
protected:

  // Used to scale the process.
  // This should be commutative :
  //   setDurationWithScale(2); setDurationWithScale(3);
  // yields the same result as :
  //   setDurationWithScale(3); setDurationWithScale(2);
  virtual void setDurationAndScale(const TimeVal& newDuration);

  // Does nothing if newDuration < currentDuration
  virtual void setDurationAndGrow(const TimeVal& newDuration);

  // Does nothing if newDuration > currentDuration
  virtual void setDurationAndShrink(const TimeVal& newDuration);

  Process::Inlets m_inlets;
  Process::Outlets m_outlets;
private:
  TimeVal m_duration;
  double m_slotHeight{}; //! Height in full view
};

SCORE_LIB_PROCESS_EXPORT ProcessModel* parentProcess(QObject* obj);
SCORE_LIB_PROCESS_EXPORT const ProcessModel*
parentProcess(const QObject* obj);
}
DEFAULT_MODEL_METADATA(Process::ProcessModel, "Process")

Q_DECLARE_METATYPE(Id<Process::ProcessModel>)
