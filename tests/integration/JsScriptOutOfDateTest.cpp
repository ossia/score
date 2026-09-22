// A document carrying its own copy of a library script can be brought back in
// line with that file: the process reports that the two differ, and hands out
// the command that replaces the copy. Nothing happens on its own -- the user
// asks for it, and can undo it.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/ProcessList.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <JS/JSProcessModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

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
)_")
      .arg(what);
}

void write(const QString& path, const QString& content)
{
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
  f.write(content.toUtf8());
}

Scenario::IntervalModel& root_interval(score::Document& doc)
{
  return score::IDocument::get<Scenario::ScenarioDocumentModel>(doc).baseInterval();
}

std::vector<JS::ProcessModel*> all_js(Scenario::IntervalModel& interval)
{
  std::vector<JS::ProcessModel*> res;
  for(auto& p : interval.processes)
    if(auto* j = qobject_cast<JS::ProcessModel*>(&p))
      res.push_back(j);
  return res;
}

JS::ProcessModel* add_js(score::Document& doc, const QString& path)
{
  auto& interval = root_interval(doc);
  auto& factories = doc.context().app.interfaces<Process::ProcessFactoryList>();
  auto* factory = factories.get(js_key);
  REQUIRE(factory != nullptr);

  CommandDispatcher<> disp{doc.context().commandStack};
  disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
      interval, factory->concreteKey(), path, QPointF{});

  auto js = all_js(interval);
  REQUIRE(!js.empty());
  return js.back();
}
}

TEST_CASE(
    "A frozen copy of a library script knows when the file moved on",
    "[integration][js][library][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir lib;
    REQUIRE(lib.isValid());
    const QString qml = lib.path() + "/greet.qml";
    write(qml, script_saying("v1"));
    write(lib.path() + "/greet.ui.qml", "import QtQuick 2.15\nItem { }\n");

    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* js = add_js(*doc, qml);
    REQUIRE(js != nullptr);
    REQUIRE(js->rootFile() == qml);

    SECTION("a script still identical to its file has nothing to update")
    {
      CHECK(!js->externalSourceOutOfDate());
      CHECK(js->refreshFromExternalSource(doc->context()) == nullptr);
    }

    SECTION("a script written in the document has no file to follow")
    {
      auto* own = add_js(*doc, {});
      REQUIRE(own != js);
      CHECK(own->rootFile().isEmpty());
      CHECK(!own->externalSourceOutOfDate());
      CHECK(own->refreshFromExternalSource(doc->context()) == nullptr);
    }

    SECTION("a copy that differs from the file can be brought back in line")
    {
      REQUIRE(js->setProgram({script_saying("frozen"), js->uiScript()}).valid);
      write(qml, script_saying("v2"));

      auto* reloaded = score::test::reload_via_json(ctx, *doc);
      REQUIRE(reloaded != nullptr);
      auto rjs = all_js(root_interval(*reloaded));
      REQUIRE(rjs.size() == 1);
      auto* proc = rjs[0];

      // The document loads exactly as it was saved: no silent update.
      CHECK(proc->executionScript().contains("frozen"));
      CHECK(proc->externalSourceOutOfDate());

      auto* cmd = proc->refreshFromExternalSource(reloaded->context());
      REQUIRE(cmd != nullptr);
      CommandDispatcher<>{reloaded->context().commandStack}.submit(cmd);

      CHECK(proc->executionScript().contains("v2"));
      CHECK(!proc->externalSourceOutOfDate());

      reloaded->commandStack().undo();

      CHECK(proc->executionScript().contains("frozen"));
      CHECK(proc->externalSourceOutOfDate());
    }

    SECTION("a file that is gone has nothing to offer")
    {
      REQUIRE(js->setProgram({script_saying("frozen"), js->uiScript()}).valid);
      REQUIRE(QFile::remove(qml));

      auto* reloaded = score::test::reload_via_json(ctx, *doc);
      REQUIRE(reloaded != nullptr);
      auto rjs = all_js(root_interval(*reloaded));
      REQUIRE(rjs.size() == 1);

      CHECK(rjs[0]->rootFile() == qml);
      CHECK(!rjs[0]->externalSourceOutOfDate());
      CHECK(rjs[0]->refreshFromExternalSource(reloaded->context()) == nullptr);
    }
  });
}
