#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/Dataflow/StringListEditor.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/LoadPresetCommand.hpp>
#include <Scenario/Commands/SetControllerControlValue.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Dataflow/Commands/EditConnection.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QApplication>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <memory>

namespace
{
const QString switch_uuid = "51083c8f-aea0-4617-b026-34fd2793c818";
const QString prompt_uuid = "a4227e94-cf7d-4776-9aa0-2f384be7d97f";
ossia::value rows(std::initializer_list<std::pair<int, std::string>> entries)
{
  std::vector<ossia::value> result;
  for(const auto& [key, text] : entries)
    result.emplace_back(std::vector<ossia::value>{key, text});
  return result;
}
Process::ControlInlet& control(Process::ProcessModel& p, int index = 1)
{
  auto inlet = qobject_cast<Process::ControlInlet*>(p.inlets()[index]);
  REQUIRE(inlet);
  return *inlet;
}
void set(
    score::Document& doc, Process::ProcessModel& p, ossia::value value, int index = 1)
{
  CommandDispatcher<>{doc.context().commandStack}
      .submit<Scenario::SetControllerControlValue>(
          control(p, index), value, doc.context());
}
void clickRowAction(QListWidget& list, int row, int action)
{
  list.doItemsLayout();
  const auto rect = list.visualItemRect(list.item(row));
  const QPointF position{
      qreal(rect.right() + 1 - (3 - action) * 20 + 10), qreal(rect.center().y())};
  QMouseEvent press{
      QEvent::MouseButtonPress,
      position,
      QPointF{list.viewport()->mapToGlobal(position.toPoint())},
      Qt::LeftButton,
      Qt::LeftButton,
      Qt::NoModifier};
  QApplication::sendEvent(list.viewport(), &press);
  QMouseEvent release{
      QEvent::MouseButtonRelease,
      position,
      QPointF{list.viewport()->mapToGlobal(position.toPoint())},
      Qt::LeftButton,
      Qt::NoButton,
      Qt::NoModifier};
  QApplication::sendEvent(list.viewport(), &release);
  QApplication::processEvents();
}

void key(QWidget& widget, int code)
{
  QKeyEvent shortcut{QEvent::ShortcutOverride, code, Qt::NoModifier};
  QApplication::sendEvent(&widget, &shortcut);
  QKeyEvent press{QEvent::KeyPress, code, Qt::NoModifier};
  QApplication::sendEvent(&widget, &press);
  QKeyEvent release{QEvent::KeyRelease, code, Qt::NoModifier};
  QApplication::sendEvent(&widget, &release);
  QApplication::processEvents();
}
}

TEST_CASE(
    "Case rows keep outlet identities and cables across edits and undo",
    "[avnd][switch][model]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto source = score::test::add_process(*doc, switch_uuid, {});
    auto sink = score::test::add_process(*doc, switch_uuid, {});
    REQUIRE(source);
    REQUIRE(sink);
    const auto initial = rows({{10000, "1"}, {10001, "2"}, {10002, "3"}});
    set(*doc, *source, initial);
    REQUIRE(source->outlets().size() == 4);
    auto first = source->outlets()[1];
    auto last = source->outlets()[3];
    auto& scenario = score::IDocument::get<Scenario::ScenarioDocumentModel>(*doc);
    CommandDispatcher<>{doc->context().commandStack}.submit<Dataflow::CreateCable>(
        scenario, Id<Process::Cable>{1}, Process::CableType::ImmediateGlutton, *last,
        *sink->inlets()[0]);
    set(*doc, *source, rows({{10002, "4"}, {10000, "1"}}));
    REQUIRE(source->outlets().size() == 3);
    CHECK(source->outlets()[1] == last);
    CHECK(source->outlets()[2] == first);
    CHECK(last->name() == "4");
    REQUIRE(scenario.cables.size() == 1);
    CHECK(scenario.cables.begin()->source().try_find(doc->context()) == last);
    doc->commandStack().undo();
    REQUIRE(source->outlets().size() == 4);
    CHECK(control(*source).value() == initial);
    CHECK(source->outlets()[3] == last);
    CHECK(scenario.cables.begin()->source().try_find(doc->context()) == last);
    set(*doc, *source, rows({{10000, "1"}}));
    CHECK(scenario.cables.empty());
    doc->commandStack().undo();
    REQUIRE(scenario.cables.size() == 1);
    CHECK(
        scenario.cables.begin()->source().try_find(doc->context())
        == source->outlets()[3]);

    auto& factories = ctx.interfaces<Process::ProcessFactoryList>();
    SECTION("binary persistence")
    {
      const auto data = DataStreamReader::marshall(*source);
      DataStream::Deserializer reader{data};
      std::unique_ptr<Process::ProcessModel> restored{
          deserialize_interface(factories, reader, doc->context(), source->parent())};
      REQUIRE(restored);
      CHECK(control(*restored).value() == initial);
      REQUIRE(restored->outlets().size() == 4);
      CHECK(restored->outlets()[3]->id() == source->outlets()[3]->id());
    }
    SECTION("JSON persistence")
    {
      JSONReader writer;
      writer.readFrom(*source);
      auto json = readJson(writer.toByteArray());
      JSONObject::Deserializer reader{json};
      std::unique_ptr<Process::ProcessModel> restored{
          deserialize_interface(factories, reader, doc->context(), source->parent())};
      REQUIRE(restored);
      CHECK(control(*restored).value() == initial);
      REQUIRE(restored->outlets().size() == 4);
      CHECK(restored->outlets()[2]->id() == source->outlets()[2]->id());
    }
    SECTION("preset renames and moves surviving keyed cables")
    {
      set(*doc, *source, rows({{10002, "renamed"}, {10000, "1"}}));
      const auto preset = source->savePreset();
      doc->commandStack().undo();
      CommandDispatcher<>{doc->context().commandStack}
          .submit<Scenario::Command::LoadPresetWithCablesBackup>(
              *source, preset, doc->context());
      REQUIRE(scenario.cables.size() == 1);
      REQUIRE(source->outlets().size() == 3);
      CHECK(
          scenario.cables.begin()->source().try_find(doc->context())
          == source->outlets()[1]);
      CHECK(source->outlets()[1]->name() == "renamed");
      doc->commandStack().undo();
      REQUIRE(scenario.cables.size() == 1);
      CHECK(
          scenario.cables.begin()->source().try_find(doc->context())
          == source->outlets()[3]);
    }
  });
}

TEST_CASE(
    "Prompt keywords migrate legacy values and weights follow their row",
    "[avnd][string-list][model]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto prompt = score::test::add_process(*doc, prompt_uuid, {});
    REQUIRE(prompt);
    control(*prompt, 0).setValue(std::string{"sky\nsea\n"});
    REQUIRE(prompt->inlets().size() == 5);
    CHECK(
        control(*prompt, 0).value()
        == rows({{12000, "sky"}, {12001, "sea"}, {12002, ""}}));
    auto sky = qobject_cast<Process::ControlInlet*>(prompt->inlets()[2]);
    auto sea = qobject_cast<Process::ControlInlet*>(prompt->inlets()[3]);
    REQUIRE(sky);
    REQUIRE(sea);
    sky->setValue(0.25f);
    sea->setValue(0.75f);
    set(*doc, *prompt, rows({{12001, "sea"}, {12000, "sky"}}), 0);
    REQUIRE(prompt->inlets().size() == 4);
    CHECK(prompt->inlets()[2] == sea);
    CHECK(prompt->inlets()[3] == sky);
    CHECK(sea->value() == ossia::value{0.75f});
    CHECK(sky->value() == ossia::value{0.25f});
    doc->commandStack().undo();
    REQUIRE(prompt->inlets().size() == 5);
    CHECK(prompt->inlets()[2] == sky);
    CHECK(sky->value() == ossia::value{0.25f});
  });
}

TEST_CASE("Native row actions synchronize the model and undo", "[avnd][string-list][ui]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto process = score::test::add_process(*doc, switch_uuid, {});
    REQUIRE(process);
    auto& inlet = control(*process);
    auto factory = ctx.interfaces<Process::PortFactoryList>().get(inlet.concreteKey());
    REQUIRE(factory);
    QGraphicsScene scene;
    QObject context;
    auto item = factory->makeControlItem(inlet, doc->context(), nullptr, &context);
    REQUIRE(item);
    scene.addItem(item);
    auto proxy = dynamic_cast<QGraphicsProxyWidget*>(item);
    REQUIRE(proxy);
    auto editor = dynamic_cast<Process::StringListEditor*>(proxy->widget());
    REQUIRE(editor);
    auto list = editor->findChild<QListWidget*>("StringListRows");
    REQUIRE(list);
    auto click = [&](const char* name) {
      auto button = editor->findChild<QPushButton*>(name);
      REQUIRE(button);
      button->click();
      QApplication::processEvents();
    };
    click("StringListAdd");
    REQUIRE(list->count() == 1);
    list->item(0)->setText("\"alpha\"");
    QApplication::processEvents();
    click("StringListAdd");
    REQUIRE(list->count() == 2);
    list->item(1)->setText("\"beta\"");
    QApplication::processEvents();
    list->setCurrentRow(0);
    clickRowAction(*list, 1, 0);
    CHECK(inlet.value() == rows({{10001, "\"beta\""}, {10000, "\"alpha\""}}));
    REQUIRE(process->outlets().size() == 3);
    CHECK(process->outlets()[1]->id().val() == 10001);
    list->setCurrentRow(1);
    clickRowAction(*list, 0, 2);
    CHECK(inlet.value() == rows({{10000, "\"alpha\""}}));
    doc->commandStack().undo();
    QApplication::processEvents();
    REQUIRE(list->count() == 2);
    CHECK(list->item(0)->text() == "\"beta\"");
    CHECK(editor->value() == inlet.value());
  });
}

TEST_CASE(
    "Reloaded legacy keyword values normalize before notification",
    "[avnd][string-list][model]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto prompt = score::test::add_process(*doc, prompt_uuid, {});
    REQUIRE(prompt);
    auto& inlet = control(*prompt, 0);
    inlet.Process::ControlInlet::setValue(std::string{"sky\nsea"});
    const auto legacy = inlet.saveData();
    inlet.setValue(rows({{12000, "changed"}}));
    inlet.loadData(legacy, Process::PortLoadDataFlags::ReloadValue);
    CHECK(inlet.value() == rows({{12000, "sky"}, {12001, "sea"}}));
    REQUIRE(prompt->inlets().size() == 4);
  });
}

TEST_CASE("Inspector labels follow case renames and undo", "[avnd][switch][ui]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto process = score::test::add_process(*doc, switch_uuid, {});
    REQUIRE(process);
    set(*doc, *process, rows({{10000, "\"old\""}}));
    Process::PortListWidget inspector{*process, doc->context(), nullptr};
    auto hasLabel = [&](const QString& text) {
      for(auto* label : inspector.findChildren<QLabel*>())
        if(label->text() == text)
          return true;
      return false;
    };
    REQUIRE(hasLabel("\"old\""));
    set(*doc, *process, rows({{10000, "\"new\""}}));
    CHECK(hasLabel("\"new\""));
    CHECK_FALSE(hasLabel("\"old\""));
    doc->commandStack().undo();
    CHECK(hasLabel("\"old\""));
    CHECK_FALSE(hasLabel("\"new\""));
  });
}

TEST_CASE(
    "Row actions preserve editing and work without selecting their row",
    "[avnd][string-list][ui]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Process::StringListEditor editor;
    editor.resize(120, 180);
    editor.setValue(rows({{10000, "first"}, {10001, "second"}, {10002, "third"}}));
    editor.show();
    QApplication::processEvents();
    auto list = editor.findChild<QListWidget*>("StringListRows");
    REQUIRE(list);
    auto add = editor.findChild<QPushButton*>("StringListAdd");
    REQUIRE(add);
    CHECK(editor.rect().contains(add->geometry()));
    int edits = 0;
    editor.on_edited = [&](ossia::value) { ++edits; };
    list->setCurrentRow(1);
    clickRowAction(*list, 0, 0);
    clickRowAction(*list, 2, 1);
    CHECK(edits == 0);
    CHECK(
        editor.value() == rows({{10000, "first"}, {10001, "second"}, {10002, "third"}}));

    clickRowAction(*list, 0, 1);
    CHECK(
        editor.value() == rows({{10001, "second"}, {10000, "first"}, {10002, "third"}}));

    list->setCurrentRow(0);
    list->setFocus();
    key(*list, Qt::Key_F2);
    auto field = list->findChild<QLineEdit*>();
    REQUIRE(field);
    field->setText("committed");
    key(*field, Qt::Key_Return);
    CHECK(
        editor.value()
        == rows({{10001, "committed"}, {10000, "first"}, {10002, "third"}}));
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    key(*list, Qt::Key_F2);
    field = list->findChild<QLineEdit*>();
    REQUIRE(field);
    field->setText("cancelled");
    key(*field, Qt::Key_Escape);
    CHECK(
        editor.value()
        == rows({{10001, "committed"}, {10000, "first"}, {10002, "third"}}));
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    key(*list, Qt::Key_F2);
    field = list->findChild<QLineEdit*>();
    REQUIRE(field);
    field->setText("before move");
    clickRowAction(*list, 0, 1);
    CHECK(
        editor.value()
        == rows({{10000, "first"}, {10001, "before move"}, {10002, "third"}}));

    list->setCurrentRow(0);
    clickRowAction(*list, 2, 2);
    CHECK(editor.value() == rows({{10000, "first"}, {10001, "before move"}}));
    add->click();
    CHECK(
        editor.value() == rows({{10000, "first"}, {10001, "before move"}, {10002, ""}}));
  });
}
