#pragma once
#include <Process/Dataflow/CableData.hpp>
#include <Process/TimeValue.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia-qt/js_utilities.hpp>

#include <QJSValue>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QVariant>

#include <score_plugin_js_export.h>

#include <memory>
#include <verdigris>
namespace score
{
struct DocumentContext;
class Command;
}
namespace Scenario::Command
{
class Macro;
}
namespace JS
{
class GlobalDeviceEnumerator;
class DeviceListener;
class SCORE_PLUGIN_JS_EXPORT EditJsContext : public QObject
{
  W_OBJECT(EditJsContext)
  using Macro = Scenario::Command::Macro;

  std::unique_ptr<Macro> m_macro;
  struct MacroClear
  {
    using Macro = Scenario::Command::Macro;
    std::unique_ptr<Macro>& macro;
    bool clearOnDelete{};
    ~MacroClear();
  };

  MacroClear macro(const score::DocumentContext& doc);

public:
  EditJsContext();
  ~EditJsContext();

  ///////////////
  /// Global ////
  ///////////////
  const score::DocumentContext* ctx();
  W_INVOKABLE(ctx);

  ///////////////
  /// Devices ///
  ///////////////
  QObject* device(QString name);
  W_SLOT(device)

  //! An existing device's settings as a JS object - {Name, Protocol} plus the
  //! protocol's own keys, as the document saves them. createDevice() accepts
  //! the same object, taking the name and protocol from its own arguments.
  QVariant deviceSettings(QString name);
  W_SLOT(deviceSettings)

  QString deviceToJson(QString addr);
  W_SLOT(deviceToJson)

  QString deviceToOSCQuery(QString addr);
  W_SLOT(deviceToOSCQuery)

  void createDevice(QString name, QString uuid, QJSValue obj);
  W_SLOT(createDevice, (QString, QString, QJSValue))

  void createDevice(QString name, QString uuid, QVariant obj);
  W_SLOT(createDevice, (QString, QString, QVariant))

  void createOSCDevice(QString name, QString host, int in, int out);
  W_SLOT(createOSCDevice)

  void connectOSCQueryDevice(QString name, QString host);
  W_SLOT(connectOSCQueryDevice)

  void removeDevice(QString name);
  W_SLOT(removeDevice)

  void createQMLWebSocketDevice(QString name, QString text);
  W_SLOT(createQMLWebSocketDevice)

  void createQMLSerialDevice(QString name, QString port, QString text);
  W_SLOT(createQMLSerialDevice)

  GlobalDeviceEnumerator* enumerateDevices();
  W_SLOT(enumerateDevices, ())

  GlobalDeviceEnumerator* enumerateDevices(const QString& protocol);
  W_SLOT(enumerateDevices, (const QString&))

  GlobalDeviceEnumerator* enumerateDevices(const QStringList& protocols);
  W_SLOT(enumerateDevices, (const QStringList&))

  DeviceListener* listenDevice(const QString& name);
  W_SLOT(listenDevice, (const QString&))

  void iterateDevice(const QString& name, const QJSValue& op);
  W_SLOT(iterateDevice, (const QString&, const QJSValue&))

  void setDeviceLearn(const QString& name, bool learn);
  W_SLOT(setDeviceLearn, (const QString&, bool))
  /////////////////
  /// Processes ///
  /////////////////
  void createAddress(QString addr, QString type);
  W_SLOT(createAddress)

  //! The unit of an existing address, spelled as the address panel shows it:
  //! "color.rgba", "position.cart2D", "" to clear. The unit is what picks the
  //! colour swatch and the XY pad over plain number fields.
  void setUnit(QString addr, QString unit);
  W_SLOT(setUnit)

  QObject* createProcess(QObject* interval, QString name, QString data);
  W_SLOT(createProcess)

  //! Number of processes hosted by an interval or a state.
  int processes(QObject* obj);
  W_SLOT(processes)

  //! The index-th process of an interval or a state. Index 0 is the most
  //! recently added one; the order is stable but is not timeline order.
  QObject* process(QObject* obj, int index);
  W_SLOT(process)

  //! The process an object belongs to: the process itself if given one, else
  //! the closest process ancestor. Ports, layers and cable endpoints all
  //! resolve through this.
  QObject* parentProcess(QObject* obj);
  W_SLOT(parentProcess)

  //! The interval an object belongs to, or null. A process gives the interval
  //! hosting it; an interval gives itself.
  QObject* parentInterval(QObject* obj);
  W_SLOT(parentInterval)

  void loadPreset(QObject* process, QString json);
  W_SLOT(loadPreset)
  QString savePreset(QObject* process);
  W_SLOT(savePreset)

  void setName(QObject* sel, QString new_name);
  W_SLOT(setName)

  //! Set an object's comment, as an undoable command.
  void setComment(QObject* sel, QString comment);
  W_SLOT(setComment)

  QObject* createBox(QObject* obj, QString startTime, QString duration, double y);
  W_SLOT(createBox, (QObject*, QString, QString, double))

  QObject*
  createBox(QObject* obj, double startTimeFlicks, double durationFlicks, double y);
  W_SLOT(createBox, (QObject*, double, double, double))

  QObject* createState(QObject* ev, double y);
  W_SLOT(createState)

  QObject* createIntervalAfter(QObject* obj, QString duration, double y);
  W_SLOT(createIntervalAfter)

  QObject* createIntervalBetween(QObject* startState, QObject* endState);
  W_SLOT(createIntervalBetween)

  // Triggers and conditions.
  // A trigger lives on a time sync; intervals (their end), states and events
  // resolve to theirs. A condition lives on an event; states resolve to theirs.
  void enableTrigger(QObject* obj);
  W_SLOT(enableTrigger)
  void disableTrigger(QObject* obj);
  W_SLOT(disableTrigger)
  void enableCondition(QObject* obj);
  W_SLOT(enableCondition)
  void disableCondition(QObject* obj);
  W_SLOT(disableCondition)
  //! The expression of what `obj` resolves to: the trigger for a time sync or an
  //! interval (its end), the condition for an event or a state. Addresses are
  //! written between % signs, e.g. "{ %osc:/x% > 0.5 }".
  void setExpression(QObject* obj, QString expression);
  W_SLOT(setExpression)
  QString expression(QObject* obj);
  W_SLOT(expression)

  void setIntervalDuration(QObject* object, TimeVal flicks);
  W_SLOT(setIntervalDuration)
  void setIntervalMinDuration(QObject* object, TimeVal flicks);
  W_SLOT(setIntervalMinDuration)
  void setIntervalMaxDuration(QObject* object, TimeVal flicks);
  W_SLOT(setIntervalMaxDuration)
  void setIntervalMaxInfinite(QObject* object, bool);
  W_SLOT(setIntervalMaxInfinite)
  void setIntervalSpeed(QObject* object, double);
  W_SLOT(setIntervalSpeed)

  void setAutoTrigger(QObject* timeSync, bool);
  W_SLOT(setAutoTrigger)

  void setProcessLoop(QObject* process, bool);
  W_SLOT(setProcessLoop)

  //! A port of a process by name, inlets first. Prefer inlet() / outlet() when
  //! a process has an input and an output sharing a name.
  QObject* port(QObject* obj, QString name);
  W_SLOT(port)

  QObject* inlet(QObject* obj, int index);
  W_SLOT(inlet, (QObject*, int))

  //! An inlet of a process by name, or null.
  QObject* inlet(QObject* obj, QString name);
  W_SLOT(inlet, (QObject*, QString))

  int inlets(QObject* obj);
  W_SLOT(inlets)

  QObject* outlet(QObject* obj, int index);
  W_SLOT(outlet, (QObject*, int))

  //! An outlet of a process by name, or null.
  QObject* outlet(QObject* obj, QString name);
  W_SLOT(outlet, (QObject*, QString))

  int outlets(QObject* obj);
  W_SLOT(outlets)

  QObject* createCable(QObject* outlet, QObject* inlet);
  W_SLOT(createCable, (QObject*, QObject*))
  QObject* createCable(QObject* outlet, QObject* inlet, Process::CableType type);
  W_SLOT(createCable, (QObject*, QObject*, Process::CableType))

  //! The index-th cable attached to a port. Works for inlets and outlets alike.
  QObject* cable(QObject* port, int index);
  W_SLOT(cable, (QObject*, int))

  //! Number of cables attached to a port. Works for inlets and outlets alike.
  int cables(QObject* port);
  W_SLOT(cables)

  //! The outlet a cable starts from, or null if it no longer resolves.
  QObject* source(QObject* cable);
  W_SLOT(source)

  //! The inlet a cable ends at, or null if it no longer resolves.
  QObject* sink(QObject* cable);
  W_SLOT(sink)

  //! The cable joining an outlet to an inlet, or null if there is none.
  QObject* cable(QObject* outlet, QObject* inlet);
  W_SLOT(cable, (QObject*, QObject*))

  void setAddress(QObject* obj, QString addr);
  W_SLOT(setAddress)

  //! Whether an audio outlet is cabled to its parent interval. An outlet
  //! writes to its address only when nothing is cabled to it.
  void setPropagate(QObject* obj, bool propagate);
  W_SLOT(setPropagate)

  void setValue(QObject* obj, double value);
  W_SLOT(setValue, (QObject*, double))

  void setValue(QObject* obj, QVector2D value);
  W_SLOT(setValue, (QObject*, QVector2D))

  void setValue(QObject* obj, QVector3D value);
  W_SLOT(setValue, (QObject*, QVector3D))

  void setValue(QObject* obj, QVector4D value);
  W_SLOT(setValue, (QObject*, QVector4D))

  void setValue(QObject* obj, QString value);
  W_SLOT(setValue, (QObject*, QString))

  void setValue(QObject* obj, bool value);
  W_SLOT(setValue, (QObject*, bool))

  void setValue(QObject* obj, int value);
  W_SLOT(setValue, (QObject*, int))

  void setValue(QObject* obj, QList<QString> value);
  W_SLOT(setValue, (QObject*, QList<QString>))

  void setValue(QObject* obj, QList<qreal> value);
  W_SLOT(setValue, (QObject*, QList<qreal>))

  void setValue(QObject* obj, QList<QVariant> value);
  W_SLOT(setValue, (QObject*, QList<QVariant>))

  QString portName(QObject* port);
  W_SLOT(portName)
  QString valueType(QObject* obj);
  W_SLOT(valueType)

  double min(QObject* obj);
  W_SLOT(min)

  double max(QObject* obj);
  W_SLOT(max)

  QVector<QVariant> enumValues(QObject* obj);
  W_SLOT(enumValues)

  QObject* metadata(QObject* obj) const noexcept;
  W_SLOT(metadata)

  //! The document's own metadata, which is not a child of anything the other
  //! accessors reach. Its fileName, author, creation and lastEdition are
  //! properties, so they read and write from QML directly.
  QObject* documentMetadata() const noexcept;
  W_SLOT(documentMetadata)

  //! What the title bar shows: the file's base name, or "Untitled..." before
  //! it has been saved anywhere.
  QString documentName() const noexcept;
  W_SLOT(documentName)

  //! Renames the document. Not a save: it sets the name the next save uses.
  void setDocumentName(QString name);
  W_SLOT(setDocumentName)

  QObject* startState(QObject* obj);
  W_SLOT(startState)

  QObject* startEvent(QObject* obj);
  W_SLOT(startEvent)

  QObject* startSync(QObject* obj);
  W_SLOT(startSync)

  QObject* endState(QObject* obj);
  W_SLOT(endState)

  QObject* endEvent(QObject* obj);
  W_SLOT(endEvent)

  QObject* endSync(QObject* obj);
  W_SLOT(endSync)

  void remove(QObject* obj);
  W_SLOT(remove)

  void setCurvePoints(QObject* process, QVector<QVariantList> points);
  W_SLOT(setCurvePoints)

  void setSteps(QObject* process, QVector<double> points);
  W_SLOT(setSteps)

  QVariantList messages(QObject* state);
  W_SLOT(messages)

  void setMessages(QObject* state, QVariantList msgs);
  W_SLOT(setMessages)

  void replaceAddress(QObjectList objects, QString before, QString after);
  W_SLOT(replaceAddress)

  //! Create automations for every parameter matching `addr` in `interval`.
  //! Returns the created processes: an address may expand to several curves.
  QVariantList automate(QObject* interval, QString addr);
  W_SLOT(automate, (QObject*, QString))

  //! Create an automation in `interval` driving `port`, and cable it up.
  //! Returns the created process, whose default curve is a 0 -> 1 ramp.
  QObject* automate(QObject* interval, QObject* port);
  W_SLOT(automate, (QObject*, QObject*))

  /////////////////
  /// Undo-redo ///
  /////////////////
  void startMacro();
  W_SLOT(startMacro)

  //! Run `fn` in a macro, committed even if the script errors. A nested call
  //! joins the enclosing macro. startMacro()/endMacro() by hand is not
  //! exception-safe, and startMacro() overwrites an open macro, rolling back
  //! what it had already applied.
  void withMacro(QJSValue fn);
  W_SLOT(withMacro)

  void endMacro();
  W_SLOT(endMacro)

  void undo();
  W_SLOT(undo)

  void redo();
  W_SLOT(redo)

  //! Undo-stack introspection. Mirroring the stack script-side desynchronises
  //! as soon as a macro commits nothing, or a document is loaded behind it.
  bool canUndo();
  W_SLOT(canUndo)

  bool canRedo();
  W_SLOT(canRedo)

  //! Number of commands that can still be undone (the stack's current index).
  int undoIndex();
  W_SLOT(undoIndex)

  //! Total number of commands on the stack.
  int undoCount();
  W_SLOT(undoCount)

  //! Description of the command the next undo()/redo() would apply, or "".
  QString undoText();
  W_SLOT(undoText)

  QString redoText();
  W_SLOT(redoText)

  void load(QString path);
  W_SLOT(load)

  void save();
  W_SLOT(save)

  void saveAs(QString path);
  W_SLOT(saveAs)

  ////////////////
  /// Document ///
  ////////////////
  QObject* find(QString p);
  W_SLOT(find)

  QObject* findByLabel(QString p);
  W_SLOT(findByLabel)
  QString path(QObject* obj);
  W_SLOT(path)
  QObject* findByPath(QString path);
  W_SLOT(findByPath)

  QObject* document();
  W_SLOT(document)

  QObject* rootInterval();
  W_SLOT(rootInterval)

  QObject* documentPlugin(QString key);
  W_SLOT(documentPlugin)

  /////////////////
  /// Execution ///
  /////////////////
  void play();
  W_SLOT(play, ())

  void play(QObject* obj);
  W_SLOT(play, (QObject*))

  void pause();
  W_SLOT(pause, ())

  void resume();
  W_SLOT(resume, ())

  void stop();
  W_SLOT(stop)

  void reinitialize();
  W_SLOT(reinitialize)

  void scrub(double z);
  W_SLOT(scrub)

  QObject* transport();
  W_INVOKABLE(transport)

  ////////////////
  /// File API ///
  QString readFile(QString path);
  W_SLOT(readFile)

  QString relativizeFilePath(QString path);
  W_SLOT(relativizeFilePath)

  QString locateFilePath(QString path);
  W_SLOT(locateFilePath)

  ////////////////
  /// Score UI ///
  ////////////////
  QObject* selectedObject();
  W_SLOT(selectedObject)

  QVariantList selectedObjects();
  W_SLOT(selectedObjects)

  void select(QObject* obj);
  W_SLOT(select, (QObject*))

  void select(QVariantList objs);
  W_SLOT(select, (QVariantList))

  void zoom(double zx, double zy);
  W_SLOT(zoom, (double, double));

  void scroll(double zx, double zy);
  W_SLOT(scroll, (double, double));

  /// Custom UI ///
  QVariant prompt(QVariant v);
  W_SLOT(prompt)

  bool hasProcessUI(QObject* process);
  W_SLOT(hasProcessUI)

  void showProcessUI(QObject* process, bool show);
  W_SLOT(showProcessUI)

  bool hasProcessScriptEditor(QObject* process);
  W_SLOT(hasProcessScriptEditor)

  void showProcessScriptEditor(QObject* process, bool show);
  W_SLOT(showProcessScriptEditor)

  //! "Window", "Side panel", "Central" or "" for the user's default
  void setProcessScriptEditorPlacement(QObject* process, QString placement);
  W_SLOT(setProcessScriptEditorPlacement)
  void setProcessUIPlacement(QObject* process, QString placement);
  W_SLOT(setProcessUIPlacement)

  /////////////////////
  /// Introspection ///
  /////////////////////
  QVariantMap availableProcesses() const noexcept;
  W_SLOT(availableProcesses)
  QVariant availableProcessesAndPresets() const noexcept;
  W_SLOT(availableProcessesAndPresets)
  QVariantList libraryEntries(QString filter) const noexcept;
  W_SLOT(libraryEntries)
  QVariant availableProtocols() const noexcept;
  W_SLOT(availableProtocols)

  QByteArray serializeAsJson() noexcept;
  W_SLOT(serializeAsJson)
private:
  void submit(Macro& m, score::Command* c);
};
}

W_REGISTER_ARGTYPE(QVector<QVariantList>)
W_REGISTER_ARGTYPE(QList<QObject*>)
W_REGISTER_ARGTYPE(JS::GlobalDeviceEnumerator*)
