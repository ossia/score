#pragma once
#include <QList>
#include <QObject>
#include <QVariant>

#include <memory>
#include <optional>
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
class EditJsContext : public QObject
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

  QString deviceToJson(QString addr);
  W_SLOT(deviceToJson)

  void createOSCDevice(QString name, QString host, int in, int out);
  W_SLOT(createOSCDevice)

  void createQMLWebSocketDevice(QString name, QString text);
  W_SLOT(createQMLWebSocketDevice)

  void createQMLSerialDevice(QString name, QString port, QString text);
  W_SLOT(createQMLSerialDevice)

  void createAddress(QString addr, QString type);
  W_SLOT(createAddress)

  QObject* createProcess(QObject* interval, QString name, QString data);
  W_SLOT(createProcess)

  void setName(QObject* sel, QString new_name);
  W_SLOT(setName)

  QObject* createBox(QObject* obj, QString startTime, QString duration, double y);
  W_SLOT(createBox)

  QObject* createIntervalAfter(QObject* obj, QString duration, double y);
  W_SLOT(createIntervalAfter)

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

  void automate(QObject* interval, QString addr);
  W_SLOT(automate, (QObject*, QString))

  void automate(QObject* interval, QObject* port);
  W_SLOT(automate, (QObject*, QObject*))

  void startMacro();
  W_SLOT(startMacro)

  void endMacro();
  W_SLOT(endMacro)

  void undo();
  W_SLOT(undo)

  void redo();
  W_SLOT(redo)

  QObject* find(QString p);
  W_SLOT(find)

  QObject* document();
  W_SLOT(document)

  /// Execution ///

  void play(QObject* obj);
  W_SLOT(play)

  void stop();
  W_SLOT(stop)

  // File API
  QString readFile(QString path);
  W_SLOT(readFile)

  QObject* selectedObject();
  W_SLOT(selectedObject)

  QVariantList selectedObjects();
  W_SLOT(selectedObjects)

  /// UI ///
  QVariant prompt(QVariant v);
  W_SLOT(prompt)

private:
  void submit(Macro& m, score::Command* c);
};
}

W_REGISTER_ARGTYPE(QVector<QVariantList>)
W_REGISTER_ARGTYPE(QList<QObject*>)
