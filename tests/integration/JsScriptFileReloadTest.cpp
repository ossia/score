// A .qml used by a Javascript process gets edited on disk, and the process is
// made again from the same file. The second one must be the script as it is
// now, not as it was the first time it was read.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/ProcessList.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <JS/Executor/ExecutionHelpers.hpp>
#include <JS/JSProcessModel.hpp>

#include <core/document/Document.hpp>

#include <QFile>
#include <QPointF>
#include <QQmlEngine>
#include <QQuickItem>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace
{
const auto js_key = UuidKey<Process::ProcessModel>::fromString(
    QStringLiteral("846a5de5-47f9-46c5-a898-013cb20951d0"));

QString script_with(int outlets)
{
  QString ports;
  for(int i = 0; i < outlets; i++)
    ports += QStringLiteral("  ValueOutlet { id: out%1 }\n").arg(i);
  return QStringLiteral("import Score 1.0\nScript {\n  ValueInlet { id: in1 }\n%1"
                        "  function onTick(oldtime, time, position, offset) { }\n}\n")
      .arg(ports);
}

void write(const QString& path, const QString& content)
{
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
  f.write(content.toUtf8());
}

JS::ProcessModel* add_js_from(score::Document& doc, const QString& path)
{
  auto& interval
      = score::IDocument::get<Scenario::ScenarioDocumentModel>(doc).baseInterval();
  auto& factories = doc.context().app.interfaces<Process::ProcessFactoryList>();
  auto* factory = factories.get(js_key);
  REQUIRE(factory != nullptr);

  const auto before = interval.processes.size();
  CommandDispatcher<> disp{doc.context().commandStack};
  disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
      interval, factory->concreteKey(), path, QPointF{});
  REQUIRE(interval.processes.size() == before + 1);

  JS::ProcessModel* last = nullptr;
  for(auto& p : interval.processes)
    if(auto* j = qobject_cast<JS::ProcessModel*>(&p))
      last = j;
  return last;
}
}

TEST_CASE("A script kept in a file is classified like the same script inline",
          "[integration][js][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    static const char* gpu_script = R"_(import Score
import QtQuick
Script {
  TextureOutlet { id: tout; objectName: "tex out" }
  tick: function(token, state) { }
})_";

    QTemporaryDir lib;
    REQUIRE(lib.isValid());
    const QString qml = lib.path() + "/tex.qml";
    write(qml, gpu_script);

    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* inlineOne = add_js_from(*doc, gpu_script);
    REQUIRE(inlineOne != nullptr);
    CHECK(inlineOne->currentExecutionObject() != nullptr);
    CHECK(inlineOne->isGpu());

    auto* fromFile = add_js_from(*doc, qml);
    REQUIRE(fromFile != nullptr);
    // Same script, same ports: the executor must route it the same way.
    CHECK(fromFile->outlets().size() == 1);
    CHECK(fromFile->currentExecutionObject() != nullptr);
    CHECK(fromFile->isGpu());
  });
}

TEST_CASE("A script file that changed on disk is read again", "[integration][js][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir lib;
    REQUIRE(lib.isValid());
    const QString qml = lib.path() + "/ports.qml";
    write(qml, script_with(1));

    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* first = add_js_from(*doc, qml);
    REQUIRE(first != nullptr);
    CHECK(first->outlets().size() == 1);

    // The user edits the file and uses it again.
    write(qml, script_with(3));

    auto* second = add_js_from(*doc, qml);
    REQUIRE(second != nullptr);
    CHECK(second->executionScript().contains("out2"));
    CHECK(second->outlets().size() == 3);

    // The one already in the document is untouched by the edit.
    CHECK(first->outlets().size() == 1);
  });
}

TEST_CASE("An edited .ui.qml is read again too", "[integration][js][gui]")
{
  // The ui half of a script is a sibling file, foo.ui.qml next to foo.qml, and
  // is compiled the same way -- it went stale for the same reason.
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    const auto ui_saying = [](const QString& name) {
      return QStringLiteral(R"_(import Score
import QtQuick
ScriptUI { objectName: "%1" })_")
          .arg(name);
    };

    QTemporaryDir lib;
    REQUIRE(lib.isValid());
    const QString qml = lib.path() + "/withui.qml";
    const QString uiQml = lib.path() + "/withui.ui.qml";
    write(qml, script_with(1));
    write(uiQml, ui_saying("first"));

    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* first = add_js_from(*doc, qml);
    REQUIRE(first != nullptr);
    REQUIRE(first->hasExternalUI());
    auto* firstItem = first->createItemForUI(doc->context());
    REQUIRE(firstItem != nullptr);
    CHECK(firstItem->objectName().toStdString() == "first");
    delete firstItem;

    write(uiQml, ui_saying("second"));

    auto* second = add_js_from(*doc, qml);
    REQUIRE(second != nullptr);
    REQUIRE(second->hasExternalUI());
    auto* secondItem = second->createItemForUI(doc->context());
    REQUIRE(secondItem != nullptr);
    CHECK(secondItem->objectName().toStdString() == "second");
    delete secondItem;
  });
}

TEST_CASE("The type loader is used for every script still in its file",
          "[integration][js][gui]")
{
  // Compiling through the loader is what lets Qt hand back an already compiled
  // script instead of parsing it again, and both the execution and the render
  // thread come through there -- with one engine shared by every node on the
  // thread, so several instances of one preset pay for it once. Skipping it
  // costs a full parse on those threads, so the conditions under which it is
  // taken are worth pinning down.
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QQmlEngine engine;
    const auto url = QUrl::fromLocalFile("/where/ever/thing.qml");
    const QByteArray v1 = "Script { }";
    const QByteArray v2 = "Script { property int x: 1 }";

    // Never compiled here: nothing stale can come back.
    CHECK(JS::detail::loaderIsCurrent(engine, url, v1));
    // Same script again: this is the hit that the whole arrangement is for.
    CHECK(JS::detail::loaderIsCurrent(engine, url, v1));
    // Changed underneath: the loader still holds the first one and would hand
    // that back, so it must not be asked.
    CHECK_FALSE(JS::detail::loaderIsCurrent(engine, url, v2));
    CHECK_FALSE(JS::detail::loaderIsCurrent(engine, url, v2));

    // Another engine has compiled nothing, and is not held back by this one.
    QQmlEngine other;
    CHECK(JS::detail::loaderIsCurrent(other, url, v2));

    // A url nobody has asked for is free regardless.
    CHECK(JS::detail::loaderIsCurrent(
        engine, QUrl::fromLocalFile("/where/ever/other.qml"), v2));
  });
}

TEST_CASE("An unmodified library script reaches the executor as its file",
          "[integration][js][gui]")
{
  // What the executor hands to the qml engine is rootPath() and qmlData(), and
  // the loader is used only when those two agree -- the file named must be the
  // script given. qmlData() is trimmed on its way into the model and a .qml
  // ends in a newline, so agreeing here means agreeing once trimmed.
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir lib;
    REQUIRE(lib.isValid());
    const QString qml = lib.path() + "/preset.qml";
    write(qml, script_with(2));

    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* js = add_js_from(*doc, qml);
    REQUIRE(js != nullptr);

    CHECK(js->rootPath() == qml);

    QFile f{qml};
    REQUIRE(f.open(QIODevice::ReadOnly));
    const QByteArray onDisk = f.readAll();
    CHECK(onDisk.trimmed() == js->qmlData().trimmed());
    // The file ends in a newline and qmlData() does not: comparing them as
    // they are is what used to send every file-backed script down the slow path.
    CHECK(onDisk != js->qmlData());
  });
}

namespace
{
QStringList stagedFoldersFor(const QString& originalFile)
{
  const QString root = JS::editStagingRoot();
  return QDir{root}.entryList(
      {JS::stagingFolderPrefix(originalFile) + "*"}, QDir::Dirs | QDir::NoDotAndDotDot);
}
}

TEST_CASE("An edited script keeps the imports of the folder it came from",
          "[integration][js][gui]")
{
  // Editing a script while the engine runs must not cost a parse on the audio
  // thread, and a parse can only be avoided -- or moved elsewhere -- for
  // something qml can reach by url. An edit is in no file, so it is given one,
  // in the cache rather than in the user's library. What it asks of the folder
  // it now sits in is sent back to the folder it was edited from.
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir lib;
    REQUIRE(lib.isValid());
    const QString qml = lib.path() + "/preset.qml";
    write(lib.path() + "/Helper.js", "function bump(x) { return x + 1; }\n");
    QDir().mkpath(lib.path() + "/Parts");
    write(lib.path() + "/Parts/Widget.qml", "import QtQuick\nItem { property int k: 7 }\n");
    write(lib.path() + "/Parts/qmldir", "Widget 1.0 Widget.qml\n");

    const auto script = [](const QString& body) {
      return QStringLiteral("import Score\nimport \"Helper.js\" as Helper\n"
                            "import \"./Parts\" as Parts\n"
                            "Script {\n  ValueInlet { id: i }\n  ValueOutlet { id: o }\n"
                            "  property int viaJs: Helper.bump(1)\n"
                            "  property var viaQmldir: Parts.Widget { }\n%1}\n").arg(body);
    };
    write(qml, script({}));

    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto* js = add_js_from(*doc, qml);
    REQUIRE(js != nullptr);
    REQUIRE(js->outlets().size() == 1);
    CHECK(stagedFoldersFor(qml).isEmpty());   // straight out of its file

    SECTION("both a .js import and a qmldir type survive the move")
    {
      REQUIRE(js->setProgram({script("  property int extra: Helper.bump(2)\n"), {}}).valid);
      CHECK(js->currentExecutionObject() != nullptr);
      CHECK(js->outlets().size() == 1);

      // it went to the cache, and the user's folder is untouched
      CHECK(stagedFoldersFor(qml).size() == 1);
      CHECK(QDir{lib.path()}.entryList({".*"}, QDir::Files | QDir::Hidden).isEmpty());
      CHECK(QDir{lib.path()}.entryList({"*.qml"}, QDir::Files).size() == 1);
    }

    SECTION("a later edit replaces what the previous one staged")
    {
      REQUIRE(js->setProgram({script("  property int a: 1\n"), {}}).valid);
      const auto first = stagedFoldersFor(qml);
      REQUIRE(first.size() == 1);

      REQUIRE(js->setProgram({script("  property int b: 2\n"), {}}).valid);
      const auto second = stagedFoldersFor(qml);
      REQUIRE(second.size() == 1);
      CHECK(second.first() != first.first());
    }

    SECTION("going back to what the file says stages nothing")
    {
      REQUIRE(js->setProgram({script("  property int a: 1\n"), {}}).valid);
      REQUIRE(stagedFoldersFor(qml).size() == 1);

      REQUIRE(js->setProgram({script({}), {}}).valid);
      CHECK(js->followsRootFile());
      CHECK(js->currentExecutionObject() != nullptr);
    }
  });
}
