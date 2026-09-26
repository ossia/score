// Default values of controls as a new process shows them.
#include <Process/Dataflow/Port.hpp>
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
