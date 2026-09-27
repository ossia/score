// Default values of controls as a new process shows them.
#include <Process/Dataflow/Port.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <Dataflow/Commands/EditConnection.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/Process.hpp>

#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <catch2/catch_all.hpp>

#include <functional>

namespace
{
Process::ControlInlet* control(Process::ProcessModel& p, const QString& name)
{
  for(auto in : p.inlets())
    if(in->name() == name)
      return qobject_cast<Process::ControlInlet*>(in);
  return nullptr;
}

//! Rewrites, in a saved document, the inlets of every process with this uuid:
//! a document saved when the process had other ports.
QByteArray editInlets(
    const QByteArray& json, const QString& uuid,
    const std::function<void(QJsonArray&)>& edit)
{
  std::function<void(QJsonValueRef)> walk;
  walk = [&](QJsonValueRef v) {
    if(v.isObject())
    {
      auto o = v.toObject();
      if(o.value("uuid").toString() == uuid && o.contains("Inlets"))
      {
        auto inlets = o.value("Inlets").toArray();
        edit(inlets);
        o["Inlets"] = inlets;
      }
      for(auto it = o.begin(); it != o.end(); ++it)
        walk(it.value());
      v = o;
    }
    else if(v.isArray())
    {
      auto a = v.toArray();
      for(auto it = a.begin(); it != a.end(); ++it)
        walk(*it);
      v = a;
    }
  };
  QJsonObject root = QJsonDocument::fromJson(json).object();
  for(auto it = root.begin(); it != root.end(); ++it)
    walk(it.value());
  return QJsonDocument{root}.toJson(QJsonDocument::Compact);
}

//! Loads \p json as a document and returns its process with the id of \p like.
Process::ProcessModel* reloadedAs(
    const score::GUIApplicationContext& app, const QByteArray& json,
    const Process::ProcessModel& like)
{
  auto& delegates = app.interfaces<score::DocumentDelegateList>();
  auto loaded = app.docManager.loadDocument(
      app, QStringLiteral("old"), json, JSONObject::type(), *delegates.begin());
  REQUIRE(loaded);
  for(auto& p : score::test::base_interval(*loaded).processes)
    if(p.id() == like.id())
      return &p;
  return nullptr;
}
}

// halp::range only has a scalar init; an xyz control with a different default
// per axis sets it in its constructor, and the control must show that.
TEST_CASE("a new camera looks from 1 1 1 at the origin", "[integration][threedim]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    auto cam = score::test::add_process(
        *doc, QStringLiteral("4c91b5e2-8d76-4ab3-9f14-6e0d8b3a2c57"), {});
    if(!cam)
      SKIP("score-plugin-threedim is not built");
    auto eye = control(*cam, QStringLiteral("Eye"));
    auto target = control(*cam, QStringLiteral("Target"));
    REQUIRE(eye);
    REQUIRE(target);
    CHECK(eye->value() == ossia::value{ossia::vec3f{1.f, 1.f, 1.f}});
    CHECK(eye->init() == ossia::value{ossia::vec3f{1.f, 1.f, 1.f}});
    CHECK(target->value() == ossia::value{ossia::vec3f{0.f, 0.f, 0.f}});
  });
}

namespace
{
const QString shell_uuid = QStringLiteral("7e4ae744-1825-4f1c-9fc9-675e41f316bc");

// Only the first n inlets of every process with this uuid, as a document
// saved when the process only had those.
QByteArray keepFirstInlets(const QByteArray& json, const QString& uuid, int n)
{
  return editInlets(json, uuid, [n](QJsonArray& inlets) {
    while(inlets.size() > n)
      inlets.removeLast();
  });
}
}

TEST_CASE(
    "the shell command has an interpreter, and older documents get it",
    "[integration][shell]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    auto sh = score::test::add_process(*doc, shell_uuid, {});
    if(!sh)
      SKIP("the shell command is not built");
    REQUIRE(sh->inlets().size() == 3);
    CHECK(sh->inlets()[0]->name() == QStringLiteral("Script"));
    auto interp = control(*sh, QStringLiteral("Interpreter"));
    auto custom = control(*sh, QStringLiteral("Custom command"));
    REQUIRE(interp);
    REQUIRE(custom);
    // A combobox, not a row of buttons.
    CHECK(qobject_cast<Process::ComboBox*>(interp));
    CHECK(custom->value() == ossia::value{std::string{"/bin/bash -c %s"}});

    control(*sh, QStringLiteral("Script"))->setValue(std::string{"echo old"});
    const auto old = keepFirstInlets(score::test::save_as_json(*doc), shell_uuid, 1);
    REQUIRE(old != score::test::save_as_json(*doc));

    auto reloaded = reloadedAs(app, old, *sh);
    REQUIRE(reloaded);
    REQUIRE(reloaded->inlets().size() == 3);
    CHECK(control(*reloaded, QStringLiteral("Script"))->value()
          == ossia::value{std::string{"echo old"}});
    CHECK(control(*reloaded, QStringLiteral("Interpreter")));
  });
}

TEST_CASE("Spigot is available as a process", "[integration][utilities]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    auto sp = score::test::add_process(
        *doc, QStringLiteral("8b75d69b-5ce4-4360-a066-c4a7f37f3353"), {});
    REQUIRE(sp);
    CHECK(control(*sp, QStringLiteral("Enabled")));
  });
}

namespace
{
// The type of a named inlet of every process with this uuid rewritten: a
// document saved when that port was of another kind.
QByteArray retypeInlet(
    const QByteArray& json, const QString& proc, const QString& port, const QString& type)
{
  return editInlets(json, proc, [&](QJsonArray& inlets) {
    for(auto i = 0; i < inlets.size(); i++)
    {
      auto in = inlets[i].toObject();
      if(in.value("Custom").toString() != port)
        continue;
      in["uuid"] = type;
      if(type == QStringLiteral("feb87e84-e0d2-428f-96ff-a123ac964f59"))
      {
        // A maintained button's value
        in["Value"] = QJsonObject{{"Bool", false}};
        in["Init"] = QJsonObject{{"Bool", false}};
        in["Domain"] = QJsonObject{{"Bool", QJsonValue{}}};
      }
      else if(type == QStringLiteral("769dd38a-bfb3-4dc6-b52a-b6abb7afe2a3"))
      {
        // A value inlet has no value of its own
        in.remove("Value");
        in.remove("Init");
        in.remove("Domain");
      }
      inlets[i] = in;
    }
  });
}
}

TEST_CASE(
    "a buffer queue saved with a held Clear and a Bang value inlet gets the impulses",
    "[integration][utilities][queue]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    const QString queue = QStringLiteral("8f68b81e-e5ba-4a10-a888-6581a5d770fe");
    auto doc = score::test::new_document(app);
    auto q = score::test::add_process(*doc, queue, {});
    REQUIRE(q);
    CHECK(qobject_cast<Process::ImpulseButton*>(control(*q, QStringLiteral("Clear"))));
    CHECK(qobject_cast<Process::ImpulseButton*>(control(*q, QStringLiteral("Bang"))));

    // A cable into Clear, which the upgrade must keep
    auto spigot = score::test::add_process(
        *doc, QStringLiteral("8b75d69b-5ce4-4360-a066-c4a7f37f3353"), {});
    REQUIRE(spigot);
    auto& dp
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate());
    CommandDispatcher<>{doc->context().commandStack}.submit(new Dataflow::CreateCable{
        dp, Id<Process::Cable>{4242}, Process::CableType::ImmediateGlutton,
        *spigot->outlets()[0], *control(*q, QStringLiteral("Clear"))});
    REQUIRE(control(*q, QStringLiteral("Clear"))->cables().size() == 1);

    auto old = score::test::save_as_json(*doc);
    old = retypeInlet(old, queue, QStringLiteral("Clear"),
                      QStringLiteral("feb87e84-e0d2-428f-96ff-a123ac964f59")); // Button
    old = retypeInlet(old, queue, QStringLiteral("Bang"),
                      QStringLiteral("769dd38a-bfb3-4dc6-b52a-b6abb7afe2a3")); // ValueInlet
    auto reloaded = reloadedAs(app, old, *q);
    REQUIRE(reloaded);
    CHECK(reloaded->inlets().size() == q->inlets().size());
    CHECK(qobject_cast<Process::ImpulseButton*>(control(*reloaded, QStringLiteral("Clear"))));
    CHECK(qobject_cast<Process::ImpulseButton*>(control(*reloaded, QStringLiteral("Bang"))));

    // The cable came through, on both ends
    auto clear = control(*reloaded, QStringLiteral("Clear"));
    REQUIRE(clear->cables().size() == 1);
    auto& loaded = score::IDocument::documentContext(*reloaded);
    auto& rdp = score::IDocument::modelDelegate<Scenario::ScenarioDocumentModel>(
        loaded.document);
    REQUIRE(rdp.cables.size() == 1);
    CHECK(&rdp.cables.begin()->sink().find(loaded) == clear);
  });
}

#include <Crousti/Executor.hpp>
#include <Crousti/ProcessModel.hpp>
#include <halp/file_port.hpp>

TEST_CASE("the file objects pick their path with a file dialog", "[integration][files]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    auto read = score::test::add_process(
        *doc, QStringLiteral("f82d1b69-c381-4eef-86c3-506d42b3d8e1"), {});
    auto line = score::test::add_process(
        *doc, QStringLiteral("0d2bd0c7-392c-45ec-bd70-6be4937f0348"), {});
    auto write = score::test::add_process(
        *doc, QStringLiteral("810761ce-52ea-4105-8bb3-598d5692c20f"), {});
    REQUIRE(read);
    REQUIRE(line);
    REQUIRE(write);
    CHECK(qobject_cast<Process::FileChooser*>(control(*read, QStringLiteral("Path"))));
    CHECK(qobject_cast<Process::FileChooser*>(control(*line, QStringLiteral("Path"))));
    // A new file cannot be picked from an open-file dialog: typed.
    CHECK(qobject_cast<Process::LineEdit*>(control(*write, QStringLiteral("Path"))));

    // A document from when the read path was a line edit keeps its path.
    control(*read, QStringLiteral("Path"))->setValue(std::string{"/data/in.txt"});
    const QString readUuid = QStringLiteral("f82d1b69-c381-4eef-86c3-506d42b3d8e1");
    auto old = retypeInlet(
        score::test::save_as_json(*doc), readUuid, QStringLiteral("Path"),
        QStringLiteral("9ae797ea-d94c-4792-acec-9ec1932bae5d")); // LineEdit
    auto reloaded = reloadedAs(app, old, *read);
    REQUIRE(reloaded);
    auto path = control(*reloaded, QStringLiteral("Path"));
    CHECK(qobject_cast<Process::FileChooser*>(path));
    CHECK(path->value() == ossia::value{std::string{"/data/in.txt"}});
  });
}

TEST_CASE("a write path expands %t and %n, a read path does not", "[integration][files]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    auto& ctx = doc->context();
    const ossia::value in{std::string{"/tmp/score-test-%t-%n.txt"}};
    auto w = oscr::resolveControlPath<halp::save_file_path<"Path">>(in, ctx);
    auto r = oscr::resolveControlPath<halp::file_path<"Path">>(in, ctx);
    const auto ws = w.get<std::string>();
    CHECK(ws.find("%t") == std::string::npos);
    CHECK(ws.find("%n") == std::string::npos);
    CHECK(ws.rfind("/tmp/score-test-", 0) == 0);
    CHECK(r.get<std::string>() == "/tmp/score-test-%t-%n.txt");
  });
}

TEST_CASE("Enumerator is available as a process", "[integration][utilities]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    auto e = score::test::add_process(
        *doc, QStringLiteral("7e998d33-864d-4483-83f4-a9db48e5703f"), {});
    REQUIRE(e);
    CHECK(control(*e, QStringLiteral("Mode")));
    CHECK(control(*e, QStringLiteral("Interval")));
  });
}
