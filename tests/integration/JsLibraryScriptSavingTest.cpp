// A Javascript process made from a library .qml is a reference to that file,
// not a copy of it. The document saves the path alone, so that the next
// version of the file -- a library update -- is what the user gets on reopen.
// Once the script is edited it stops being that file and the document must
// carry the edited text instead.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/ProcessList.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <JS/JSProcessModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/document/Document.hpp>

#include <QDir>
#include <QFile>
#include <QPointF>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace
{
const auto js_key = UuidKey<Process::ProcessModel>::fromString(
    QStringLiteral("846a5de5-47f9-46c5-a898-013cb20951d0"));

QString script_saying(const QString& what)
{
  return QStringLiteral(R"_(import Score 1.0
Script {
  ValueInlet { id: in1 }
  ValueOutlet { id: out1 }
  property string origin: "%1"
  function onTick(oldtime, time, position, offset) { }
}
)_").arg(what);
}

void write(const QString& path, const QString& content)
{
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
  f.write(content.toUtf8());
}

JS::ProcessModel* only_js(Scenario::IntervalModel& interval)
{
  for(auto& p : interval.processes)
    if(auto* j = qobject_cast<JS::ProcessModel*>(&p))
      return j;
  return nullptr;
}

Scenario::IntervalModel& root_interval(score::Document& doc)
{
  return score::IDocument::get<Scenario::ScenarioDocumentModel>(doc).baseInterval();
}

JS::ProcessModel* add_js_from(score::Document& doc, const QString& path)
{
  auto& interval = root_interval(doc);
  auto& factories = doc.context().app.interfaces<Process::ProcessFactoryList>();
  auto* factory = factories.get(js_key);
  REQUIRE(factory != nullptr);

  CommandDispatcher<> disp{doc.context().commandStack};
  disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
      interval, factory->concreteKey(), path, QPointF{});
  return only_js(interval);
}
}

TEST_CASE("A library script is saved as its path until it is edited",
          "[integration][js][serialization][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir lib;
    REQUIRE(lib.isValid());
    const QString qml = lib.path() + "/greet.qml";
    const QString uiQml = lib.path() + "/greet.ui.qml";
    write(qml, script_saying("v1"));
    write(uiQml, "import QtQuick 2.15\nItem { }\n");

    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* js = add_js_from(*doc, qml);
    REQUIRE(js != nullptr);
    CHECK(js->rootFile() == qml);
    CHECK(js->followsRootFile());
    CHECK(js->executionScript().contains("v1"));

    SECTION("the saved document names the file instead of quoting it")
    {
      const QByteArray saved = score::test::save_as_json(*doc);
      CHECK(saved.contains("greet.qml"));
      CHECK(!saved.contains("function onTick"));

      // The point of all this: the file moves on, and the document follows.
      write(qml, script_saying("v2"));

      auto* reloaded = score::test::reload_via_json(ctx, *doc);
      REQUIRE(reloaded != nullptr);
      auto* rjs = only_js(root_interval(*reloaded));
      REQUIRE(rjs != nullptr);
      CHECK(rjs->followsRootFile());
      CHECK(rjs->executionScript().contains("v2"));
      CHECK(!rjs->executionScript().contains("v1"));
    }

    SECTION("an edited script is carried by the document itself")
    {
      const auto edited = script_saying("edited-here");
      REQUIRE(js->setProgram({edited, js->uiScript()}).valid);
      CHECK(!js->followsRootFile());
      // The file it came from is still known: it is the script that diverged.
      CHECK(js->rootFile() == qml);

      const QByteArray saved = score::test::save_as_json(*doc);
      CHECK(saved.contains("edited-here"));

      // A library update must not overwrite what the user wrote.
      write(qml, script_saying("v2"));

      auto* reloaded = score::test::reload_via_json(ctx, *doc);
      REQUIRE(reloaded != nullptr);
      auto* rjs = only_js(root_interval(*reloaded));
      REQUIRE(rjs != nullptr);
      CHECK(!rjs->followsRootFile());
      CHECK(rjs->executionScript().contains("edited-here"));
    }

    SECTION("the binary format keeps the same distinction")
    {
      auto* reloaded = score::test::reload_via_bytes(ctx, *doc);
      REQUIRE(reloaded != nullptr);
      auto* rjs = only_js(root_interval(*reloaded));
      REQUIRE(rjs != nullptr);
      CHECK(rjs->followsRootFile());
      CHECK(rjs->executionScript().contains("v1"));
    }
  });
}
