// SPDX-License-Identifier: GPL-3.0-or-later
//
// An avendish process whose spec gained a port since a document was saved has
// its ports rebuilt on load: the saved ones are matched back by (name, type)
// and the new one comes up at its declared default. Nothing is lost there, so
// nothing is reported; only a saved port that matches nothing -- renamed or
// removed -- costs the user something, and that is what gets a warning.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <State/Address.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/serialization/JSONVisitor.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <catch2/catch_test_macros.hpp>

#include <functional>

namespace
{
const QString textbox_uuid = QStringLiteral("7be51631-fb4b-4152-9ca7-86fdafa8a989");
const QString saved_text = QStringLiteral("a comment from an older score");

//! Rewrite every process object carrying `uuid` with `fn`.
QJsonValue rewrite_process(
    const QJsonValue& v, const QString& uuid,
    const std::function<QJsonObject(QJsonObject)>& fn)
{
  if(v.isObject())
  {
    QJsonObject o = v.toObject();
    if(o.value("uuid").toString() == uuid)
      return fn(o);
    for(const auto& k : o.keys())
      o[k] = rewrite_process(o.value(k), uuid, fn);
    return o;
  }
  if(v.isArray())
  {
    QJsonArray a;
    for(const auto& e : v.toArray())
      a.push_back(rewrite_process(e, uuid, fn));
    return a;
  }
  return v;
}

//! The messages score prints while a document loads.
struct message_capture
{
  static inline QStringList messages;

  message_capture()
      : prev{qInstallMessageHandler(&handler)}
  {
    messages.clear();
  }
  ~message_capture() { qInstallMessageHandler(prev); }

  static void handler(QtMsgType, const QMessageLogContext&, const QString& msg)
  {
    messages.push_back(msg);
  }

  static bool sawPortLoss() { return messages.filter("ports lost on load").size() > 0; }

  QtMessageHandler prev{};
};

Process::ControlInlet* inlet_named(Process::ProcessModel& proc, const QString& name)
{
  for(auto* inl : proc.inlets())
    if(inl->name() == name)
      return qobject_cast<Process::ControlInlet*>(inl);
  return nullptr;
}

Process::ProcessModel* textbox_in(score::Document& doc)
{
  const auto key = UuidKey<Process::ProcessModel>::fromString(textbox_uuid);
  for(auto& p : score::test::base_interval(doc).processes)
    if(p.concreteKey() == key)
      return &p;
  return nullptr;
}

//! A saved document whose comment box has the ports `fn` leaves it.
QByteArray saved_textbox(
    const score::GUIApplicationContext& ctx,
    const std::function<QJsonObject(QJsonObject)>& fn)
{
  auto* doc = score::test::new_document(ctx);
  REQUIRE(doc != nullptr);

  auto* proc = score::test::add_process(*doc, textbox_uuid, QString{});
  if(!proc)
    return {};

  REQUIRE(proc->inlets().size() == 2);
  auto* text = inlet_named(*proc, QStringLiteral("in"));
  REQUIRE(text != nullptr);
  text->setValue(ossia::value{saved_text.toStdString()});
  text->setAddress(
      State::AddressAccessor{*State::Address::fromString(QStringLiteral("dev:/text"))});

  const QByteArray json = score::test::save_as_json(*doc);
  ctx.docManager.forceCloseDocument(ctx, *doc);
  QApplication::processEvents();

  auto rewritten
      = rewrite_process(QJsonDocument::fromJson(json).object(), textbox_uuid, fn);
  return QJsonDocument{rewritten.toObject()}.toJson();
}

score::Document* load(const score::GUIApplicationContext& ctx, const QByteArray& json)
{
  auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
  REQUIRE(!delegates.empty());
  auto* doc = ctx.docManager.loadDocument(
      ctx, QStringLiteral("port-drift"), json, JSONObject::type(), *delegates.begin());
  QApplication::processEvents();
  return doc;
}
}

TEST_CASE(
    "a process that gained a port since the save loads back quietly",
    "[integration][ports][avnd]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // The document as it was written before "Auto-reflow" existed.
    const QByteArray json = saved_textbox(ctx, [](QJsonObject proc) {
      QJsonArray kept;
      for(const auto& inl : proc.value("Inlets").toArray())
        if(inl.toObject().value("Custom").toString() != QStringLiteral("Auto-reflow"))
          kept.push_back(inl);
      REQUIRE(kept.size() == 1);
      proc["Inlets"] = kept;
      return proc;
    });
    if(json.isEmpty())
      return; // score-plugin-ui is not in this build

    message_capture capture;
    auto* reloaded = load(ctx, json);
    REQUIRE(reloaded != nullptr);

    auto* proc = textbox_in(*reloaded);
    REQUIRE(proc != nullptr);
    REQUIRE(proc->inlets().size() == 2);

    // The port that was saved keeps everything it had...
    auto* text = inlet_named(*proc, QStringLiteral("in"));
    REQUIRE(text != nullptr);
    CHECK(score::test::control_string(*text) == saved_text);
    CHECK(text->address().address.toString() == QStringLiteral("dev:/text"));

    // ...and the one the spec gained comes up at its declared default.
    auto* reflow = inlet_named(*proc, QStringLiteral("Auto-reflow"));
    REQUIRE(reflow != nullptr);
    CHECK(ossia::convert<bool>(reflow->value()) == false);

    // Nothing at all is printed about a process that came back intact.
    CHECK(message_capture::messages.filter(QStringLiteral("Text Box")).isEmpty());
  });
}

TEST_CASE(
    "a saved port that matches nothing in the new spec is reported",
    "[integration][ports][avnd]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // Same drift, but the surviving port was also renamed: its value has
    // nowhere to go.
    const QByteArray json = saved_textbox(ctx, [](QJsonObject proc) {
      QJsonArray kept;
      for(const auto& v : proc.value("Inlets").toArray())
      {
        QJsonObject inl = v.toObject();
        if(inl.value("Custom").toString() == QStringLiteral("Auto-reflow"))
          continue;
        inl["Custom"] = QStringLiteral("in_from_an_older_spec");
        kept.push_back(inl);
      }
      REQUIRE(kept.size() == 1);
      proc["Inlets"] = kept;
      return proc;
    });
    if(json.isEmpty())
      return;

    message_capture capture;
    auto* reloaded = load(ctx, json);
    REQUIRE(reloaded != nullptr);

    auto* proc = textbox_in(*reloaded);
    REQUIRE(proc != nullptr);
    REQUIRE(proc->inlets().size() == 2);

    auto* text = inlet_named(*proc, QStringLiteral("in"));
    REQUIRE(text != nullptr);
    CHECK(score::test::control_string(*text) != saved_text);

    REQUIRE(message_capture::sawPortLoss());
    CHECK(
        message_capture::messages.filter("in_from_an_older_spec").size() > 0);
  });
}

TEST_CASE(
    "a control renamed since the save gets its value back, its choices by their place",
    "[integration][ports][avnd][looper]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    const QString looper_uuid = QStringLiteral("a0ad4227-ac3d-448b-a19b-19581ed4e2c6");
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto* proc = score::test::add_process(*doc, looper_uuid, QString{});
    if(!proc)
      return; // score-plugin-fx is not in this build
    // The audio looper as it was saved before After record and Start from had
    // those names: its first inlets only, Post-action and Restart.
    inlet_named(*proc, QStringLiteral("After record"))
        ->setValue(ossia::value{std::string{"Overdub"}});
    inlet_named(*proc, QStringLiteral("Start from"))
        ->setValue(ossia::value{std::string{"Recording"}});
    const QByteArray saved = score::test::save_as_json(*doc);
    ctx.docManager.forceCloseDocument(ctx, *doc);
    QApplication::processEvents();

    const auto rewritten = rewrite_process(
        QJsonDocument::fromJson(saved).object(), looper_uuid, [](QJsonObject p) {
      QJsonArray kept;
      for(const auto& v : p.value("Inlets").toArray())
      {
        if(kept.size() == 7)
          break;
        QJsonObject inl = v.toObject();
        if(inl.value("Custom").toString() == QStringLiteral("After record"))
          inl["Custom"] = QStringLiteral("Post-action");
        if(inl.value("Custom").toString() == QStringLiteral("Start from"))
          inl["Custom"] = QStringLiteral("Restart");
        kept.push_back(inl);
      }
      p["Inlets"] = kept;
      return p;
    });

    message_capture capture;
    auto* reloaded = load(ctx, QJsonDocument{rewritten.toObject()}.toJson());
    REQUIRE(reloaded != nullptr);
    Process::ProcessModel* looper{};
    const auto key = UuidKey<Process::ProcessModel>::fromString(looper_uuid);
    for(auto& p : score::test::base_interval(*reloaded).processes)
      if(p.concreteKey() == key)
        looper = &p;
    REQUIRE(looper != nullptr);

    auto* after = inlet_named(*looper, QStringLiteral("After record"));
    REQUIRE(after != nullptr);
    CHECK(score::test::control_string(*after) == "Overdub");
    // Restart's Recording is what Where it was is now.
    auto* from = inlet_named(*looper, QStringLiteral("Start from"));
    REQUIRE(from != nullptr);
    CHECK(score::test::control_string(*from) == "Where it was");
    CHECK(!message_capture::sawPortLoss());
  });
}

TEST_CASE(
    "a looper saved when Export was called Takes keeps what it wrote",
    "[integration][ports][avnd][looper]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // The audio looper, then the MIDI looper.
    for(const QString looper_uuid :
        {QStringLiteral("a0ad4227-ac3d-448b-a19b-19581ed4e2c6"),
         QStringLiteral("0c0b7d3e-5a52-4b8f-9d1f-6e3a2b8c4f71")})
    {
      INFO(looper_uuid.toStdString());
      auto* doc = score::test::new_document(ctx);
      REQUIRE(doc != nullptr);
      auto* proc = score::test::add_process(*doc, looper_uuid, QString{});
      if(!proc)
        return; // score-plugin-fx is not in this build
      auto* exported = inlet_named(*proc, QStringLiteral("Export"));
      REQUIRE(exported != nullptr);
      exported->setValue(ossia::value{2});
      const QByteArray saved = score::test::save_as_json(*doc);
      ctx.docManager.forceCloseDocument(ctx, *doc);
      QApplication::processEvents();

      const auto rewritten = rewrite_process(
          QJsonDocument::fromJson(saved).object(), looper_uuid, [](QJsonObject p) {
        QJsonArray inlets;
        for(const auto& v : p.value("Inlets").toArray())
        {
          QJsonObject inl = v.toObject();
          if(inl.value("Custom").toString() == QStringLiteral("Export"))
            inl["Custom"] = QStringLiteral("Takes");
          inlets.push_back(inl);
        }
        p["Inlets"] = inlets;
        return p;
      });

      message_capture capture;
      auto* reloaded = load(ctx, QJsonDocument{rewritten.toObject()}.toJson());
      REQUIRE(reloaded != nullptr);
      Process::ProcessModel* looper{};
      const auto key = UuidKey<Process::ProcessModel>::fromString(looper_uuid);
      for(auto& p : score::test::base_interval(*reloaded).processes)
        if(p.concreteKey() == key)
          looper = &p;
      REQUIRE(looper != nullptr);
      auto* back = inlet_named(*looper, QStringLiteral("Export"));
      REQUIRE(back != nullptr);
      // Both, the third of Mix, Layers, Both.
      CHECK(ossia::convert<int>(back->value()) == 2);
      CHECK(inlet_named(*looper, QStringLiteral("Takes")) == nullptr);
      CHECK(!message_capture::sawPortLoss());
      ctx.docManager.forceCloseDocument(ctx, *reloaded);
      QApplication::processEvents();
    }
  });
}
