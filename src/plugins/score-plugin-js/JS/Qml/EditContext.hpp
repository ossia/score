#pragma once
#include <score/document/DocumentContext.hpp>

#include <ossia-qt/js_utilities.hpp>

#include <QJSValue>
#include <QList>
#include <QObject>
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

  const score::DocumentContext* ctx();
  W_INVOKABLE(ctx);

  ///////////////
  /// Devices ///
  ///////////////
  QString deviceToJson(QString addr);
  W_SLOT(deviceToJson)

  QString deviceToOSCQuery(QString addr);
  W_SLOT(deviceToOSCQuery)

  void createDevice(QString name, QString uuid, QJSValue obj);
  W_SLOT(createDevice)

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

  GlobalDeviceEnumerator* enumerateDevices(const QString& uuid);
  W_SLOT(enumerateDevices, (const QString&))

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

  QObject* createProcess(QObject* interval, QString name, QString data);
  W_SLOT(createProcess)

  void setName(QObject* sel, QString new_name);
  W_SLOT(setName)

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

  QObject* port(QObject* obj, QString name);
  W_SLOT(port)

  QObject* inlet(QObject* obj, int index);
  W_SLOT(inlet)

  int inlets(QObject* obj);
  W_SLOT(inlets)

  QObject* outlet(QObject* obj, int index);
  W_SLOT(outlet)

  int outlets(QObject* obj);
  W_SLOT(outlets)

  void setAddress(QObject* obj, QString addr);
  W_SLOT(setAddress)

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

  void setValue(QObject* obj, QList<QString> value);
  W_SLOT(setValue, (QObject*, QList<QString>))

  QString valueType(QObject* obj);
  W_SLOT(valueType)

  double min(QObject* obj);
  W_SLOT(min)

  double max(QObject* obj);
  W_SLOT(max)

  QVector<QString> enumValues(QObject* obj);
  W_SLOT(enumValues)

  QObject* metadata(QObject* obj) const noexcept;
  W_SLOT(metadata)

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

  void automate(QObject* interval, QString addr);
  W_SLOT(automate, (QObject*, QString))

  void automate(QObject* interval, QObject* port);
  W_SLOT(automate, (QObject*, QObject*))

  /////////////////
  /// Undo-redo ///
  /////////////////
  void startMacro();
  W_SLOT(startMacro)

  void endMacro();
  W_SLOT(endMacro)

  void undo();
  W_SLOT(undo)

  void redo();
  W_SLOT(redo)

  ////////////////
  /// Document ///
  ////////////////
  QObject* find(QString p);
  W_SLOT(find)

  QObject* findByLabel(QString p);
  W_SLOT(findByLabel)

  QObject* document();
  W_SLOT(document)

  QObject* rootInterval();
  W_SLOT(rootInterval)

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

  void scrub(double z);
  W_SLOT(scrub)

  ////////////////
  /// File API ///
  QString readFile(QString path);
  W_SLOT(readFile)

  ////////////////
  /// Score UI ///
  ////////////////
  QObject* selectedObject();
  W_SLOT(selectedObject)

  QVariantList selectedObjects();
  W_SLOT(selectedObjects)

  void zoom(double zx, double zy);
  W_SLOT(zoom, (double, double));

  void scroll(double zx, double zy);
  W_SLOT(scroll, (double, double));

  /// Custom UI ///
  QVariant prompt(QVariant v);
  W_SLOT(prompt)

  /////////////////////
  /// Introspection ///
  /////////////////////
  QVariant availableProcesses() const noexcept;
  W_SLOT(availableProcesses)
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
