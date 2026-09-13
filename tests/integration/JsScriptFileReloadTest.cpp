// A .qml used by a Javascript process gets edited on disk, and the process is
// made again from the same file. The second one must be the script as it is
// now, not as it was the first time it was read.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/ProcessList.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <JS/JSProcessModel.hpp>

#include <core/document/Document.hpp>

#include <QFile>
#include <QPointF>
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
      .arg(ports)
      .trimmed();
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
