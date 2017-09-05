#pragma once
#include <Process/ExpandMode.hpp>

#include <Process/TimeValue.hpp>
#include <QByteArray>
#include <QString>
#include <iscore/model/Entity.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <vector>

#include <iscore/model/Component.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/plugins/customfactory/SerializableInterface.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/tools/Metadata.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore_lib_process_export.h>

class ProcessStateDataInterface;

namespace Process
{
class ProcessModelFactory;
class LayerFactory;
class ProcessModel;
class LayerFactory;

/**
 * @brief The Process class
 *
 * Interface to implement to make a process.
 */
class ISCORE_LIB_PROCESS_EXPORT ProcessModel
    : public iscore::Entity<ProcessModel>,
    public iscore::SerializableInterface<ProcessModel>
{
  Q_OBJECT

  ISCORE_SERIALIZE_FRIENDS

public:
  ProcessModel(
      TimeVal duration,
      const Id<ProcessModel>& id,
      const QString& name,
      QObject* parent);

  ProcessModel(DataStream::Deserializer& vis, QObject* parent);
  ProcessModel(JSONObject::Deserializer& vis, QObject* parent);

  virtual ~ProcessModel();

  virtual ProcessModel*
  clone(const Id<Process::ProcessModel>& newId, QObject* newParent) const = 0;

  // A user-friendly text to show to the users
  virtual QString prettyName() const;
  virtual QString prettyShortName() const = 0;

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

signals:
  // True if the execution is running.
  void execution(bool);
  void durationChanged(const TimeVal&);
  void useParentDurationChanged(bool);

  void slotHeightChanged(double);
  void prettyNameChanged();

protected:
  // Clone
  ProcessModel(
      const ProcessModel& other,
      const Id<ProcessModel>& id,
      const QString& name,
      QObject* parent);

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

private:
  TimeVal m_duration;
  double m_slotHeight{}; //! Height in full view
};

ISCORE_LIB_PROCESS_EXPORT ProcessModel* parentProcess(QObject* obj);
ISCORE_LIB_PROCESS_EXPORT const ProcessModel*
parentProcess(const QObject* obj);
}
DEFAULT_MODEL_METADATA(Process::ProcessModel, "Process")

Q_DECLARE_METATYPE(Id<Process::ProcessModel>)
