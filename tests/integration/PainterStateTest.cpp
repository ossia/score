#include <Process/ProcessList.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <JS/JSProcessModel.hpp>
#include <Library/LibrarySettings.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/command/CommandStack.hpp>

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QStandardPaths>
#include <QThread>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

namespace
{
QString painterDirectory(const score::GUIApplicationContext& ctx)
{
  QStringList candidates;
  if(const auto path = qEnvironmentVariable("SCORE_JS_PRESETS_DIR"); !path.isEmpty())
    candidates.push_back(path);
  else
    candidates
        = {ctx.settings<Library::Settings::Model>().getDefaultLibraryPath()
               + "/Presets/Javascript",
           QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
               + "/ossia/score/packages/default/Presets/Javascript"};
  for(const auto& path : candidates)
    if(QFileInfo{path}.isDir())
      return QDir{path}.filePath("canvas-painter");
  SKIP("Javascript presets are not installed; set SCORE_JS_PRESETS_DIR");
  return {};
}

template <typename... Args>
QVariant invoke(QObject& item, const char* method, const Args&... args)
{
  QVariant result;
  INFO(method);
  REQUIRE(
      QMetaObject::invokeMethod(
          &item, method, Qt::DirectConnection, qReturnArg(result),
          QVariant::fromValue(args)...));
  return result;
}

QJsonObject json(const QString& text)
{
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(text.toUtf8(), &error);
  REQUIRE(error.error == QJsonParseError::NoError);
  REQUIRE(document.isObject());
  return document.object();
}

QJsonObject editorDocument(const QQuickItem& editor)
{
  return json(editor.property("serialized").toString());
}

QJsonObject processDocument(const JS::ProcessModel& process)
{
  const auto it = process.state().find(QStringLiteral("paintDoc"));
  REQUIRE(it != process.state().end());
  return json(QString::fromStdString(it->second.get<std::string>()));
}

template <typename Predicate>
void eventually(Predicate&& predicate)
{
  QElapsedTimer timer;
  timer.start();
  while(timer.elapsed() < 3000)
  {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    if(predicate())
      return;
    QThread::msleep(2);
  }
  REQUIRE(predicate());
}

JS::ProcessModel& painterProcess(score::Document& doc)
{
  auto& interval
      = static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate())
            .baseInterval();
  JS::ProcessModel* painter{};
  for(auto& process : interval.processes)
    if((painter = qobject_cast<JS::ProcessModel*>(&process)))
      break;
  REQUIRE(painter != nullptr);
  return *painter;
}
}

TEST_CASE(
    "Paint documents preserve palette edits and migrate legacy brush settings",
    "[unit][js][painting]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    const auto directory = painterDirectory(ctx);
    QQmlEngine engine;
    QQmlComponent component{&engine};
    component.setData(
        R"QML(import QtQml
import "PaintModel.js" as Model
QtObject {
    function parseDocument(value) { return JSON.stringify(Model.parse(value)); }
    function newDocument() { return JSON.stringify(Model.defaults()); }
}
)QML",
        QUrl::fromLocalFile(directory + "/StateTest.qml"));
    INFO(component.errorString().toStdString());
    REQUIRE(component.isReady());
    std::unique_ptr<QObject> module{component.create()};
    REQUIRE(module != nullptr);
    const auto original = json(invoke(*module, "newDocument").toString());
    auto palette = original["palette"].toArray();
    palette[2] = QStringLiteral("#8040c080");
    auto edited = original;
    edited["palette"] = palette;
    auto config = edited["config"].toObject();
    config["feather"] = 0.65;
    edited["config"] = config;
    const auto parsed = json(
        invoke(
            *module, "parseDocument", QString::fromUtf8(QJsonDocument{edited}.toJson()))
            .toString());
    CHECK(parsed["palette"] == palette);
    CHECK(parsed["config"].toObject()["feather"].toDouble() == 0.65);

    const QString legacy
        = R"JSON({"version":1,"config":{"tool":"rectangle","filled":true},"strokes":[{"tool":"brush","color":"#ff0000","width":20,"opacity":1,"points":[[20,30]]}],"redo":[{"tool":"clear"}]})JSON";
    const auto migrated = json(invoke(*module, "parseDocument", legacy).toString());
    CHECK(migrated["strokes"] == json(legacy)["strokes"]);
    CHECK(migrated["config"].toObject()["feather"].toDouble() == 0.);
    CHECK(migrated["config"].toObject()["filled"].toBool());
    CHECK(migrated["palette"] == original["palette"]);
    CHECK_FALSE(migrated.contains("redo"));
  });
}

TEST_CASE(
    "Painter edits use score history and survive document serialization",
    "[integration][js][painting][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    const auto directory = painterDirectory(ctx);
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    const auto key = UuidKey<Process::ProcessModel>::fromString(
        QStringLiteral("846a5de5-47f9-46c5-a898-013cb20951d0"));
    auto* factory = ctx.interfaces<Process::ProcessFactoryList>().get(key);
    REQUIRE(factory != nullptr);
    CommandDispatcher<> dispatcher{doc->context().commandStack};
    dispatcher.submit<Scenario::Command::AddOnlyProcessToInterval>(
        interval, factory->concreteKey(), factory->customConstructionData(), QPointF{});
    auto& process = painterProcess(*doc);
    REQUIRE(process.currentExecutionObject() != nullptr);
    auto* engine = qmlEngine(process.currentExecutionObject());
    REQUIRE(engine != nullptr);
    engine->addImportPath(QDir{directory}.absoluteFilePath("../../../Scripts/include"));
    REQUIRE(process
                .setProgram(
                    {directory + "/canvas-painter.qml",
                     directory + "/canvas-painter.ui.qml"})
                .valid);
    std::unique_ptr<QQuickItem> editor{process.createItemForUI(doc->context())};
    REQUIRE(editor != nullptr);
    const auto originalPalette = editorDocument(*editor)["palette"].toArray();

    invoke(*editor, "setPaletteColor", 2, QStringLiteral("#8040c080"));
    CHECK(processDocument(process)["palette"].toArray()[2].toString() == "#8040c080");
    doc->commandStack().undo();
    eventually(
        [&] { return editorDocument(*editor)["palette"].toArray() == originalPalette; });
    doc->commandStack().redo();
    eventually([&] {
      return editorDocument(*editor)["palette"].toArray()[2].toString() == "#8040c080";
    });

    invoke(*editor, "setConfig", QStringLiteral("feather"), 0.65);
    invoke(*editor, "setTool", QStringLiteral("ellipse"), true);
    CHECK(processDocument(process)["config"].toObject()["filled"].toBool());
    doc->commandStack().undo();
    eventually([&] {
      const auto config = editorDocument(*editor)["config"].toObject();
      return config["tool"].toString() == "brush" && !config["filled"].toBool();
    });
    CHECK(editorDocument(*editor)["config"].toObject()["feather"].toDouble() == 0.65);
    doc->commandStack().redo();
    eventually(
        [&] { return editorDocument(*editor)["config"].toObject()["filled"].toBool(); });

    const auto saved = processDocument(process);
    auto* restoredDoc = score::test::reload_via_bytes(ctx, *doc);
    REQUIRE(restoredDoc != nullptr);
    auto& restored = painterProcess(*restoredDoc);
    CHECK(processDocument(restored) == saved);
    std::unique_ptr<QQuickItem> restoredEditor{
        restored.createItemForUI(restoredDoc->context())};
    REQUIRE(restoredEditor != nullptr);
    CHECK(editorDocument(*restoredEditor) == saved);
    invoke(*restoredEditor, "setPaletteColor", 2, QStringLiteral("#123456"));
    restoredDoc->commandStack().undo();
    eventually(
        [&] { return editorDocument(*restoredEditor)["palette"] == saved["palette"]; });
  });
}
