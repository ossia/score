// Presets of a Javascript process: the program, the controls and the state
// set through Script.replaceState are saved, loaded and undone together; a
// script loaded from a file is saved as a reference to that file; presets
// without a state still load.

#include <Process/Commands/LoadPresetCommandFactory.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Preset.hpp>
#include <Process/ProcessList.hpp>

#include <JS/JSProcessModel.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <score/serialization/JSONVisitor.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

namespace
{
const QString js_uuid = QStringLiteral("846a5de5-47f9-46c5-a898-013cb20951d0");

constexpr auto script = R"_(import Score
Script {
  FloatSlider { objectName: "level"; min: 0; max: 10; init: 1 }
  tick: function(token, state) { }
})_";

JS::ProcessModel& addScript(score::Document& doc)
{
  auto js = qobject_cast<JS::ProcessModel*>(score::test::add_process(doc, js_uuid, {}));
  REQUIRE(js);
  (void)js->setProgram(JS::QmlSource{script, {}});
  REQUIRE(js->inlets().size() == 1);
  return *js;
}

Process::ControlInlet& level(JS::ProcessModel& js)
{
  auto c = qobject_cast<Process::ControlInlet*>(js.inlets()[0]);
  REQUIRE(c);
  return *c;
}

Process::Preset save(const JS::ProcessModel& js)
{
  return static_cast<const Process::ProcessModel&>(js).savePreset();
}

bool matches(const JS::ProcessModel& js, const Process::Preset& p)
{
  return static_cast<const Process::ProcessModel&>(js).presetMatches(p);
}

void load(JS::ProcessModel& js, const Process::Preset& p)
{
  static_cast<Process::ProcessModel&>(js).loadPreset(p);
}

const JS::JSState stateA{
    {QStringLiteral("step"), ossia::value{3}},
    {QStringLiteral("pattern"), ossia::value{std::string{"x..x"}}}};
const JS::JSState stateB{{QStringLiteral("step"), ossia::value{7}}};

constexpr auto otherScript = R"_(import Score
Script {
  FloatSlider { objectName: "gain"; min: 0; max: 10; init: 1 }
  FloatSlider { objectName: "mix"; min: 0; max: 1; init: 0.5 }
  tick: function(token, state) { }
})_";
constexpr auto otherUi = R"_(import QtQuick
Item { })_";

void loadAsCommand(
    const score::GUIApplicationContext& ctx, score::Document& doc, JS::ProcessModel& js,
    const Process::Preset& preset)
{
  auto& factories = ctx.interfaces<Process::LoadPresetCommandFactoryList>();
  auto cmd = factories.make(
      &Process::LoadPresetCommandFactory::make, js, preset, doc.context());
  REQUIRE(cmd);
  CommandDispatcher<>{doc.context().commandStack}.submit(cmd);
}

void write(const QString& path, const char* text)
{
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
  f.write(text);
}

// Included by the script: gives the name of its first control
constexpr auto helper = R"_(.pragma library
var name = "fromhelper";
)_";
constexpr auto fileScript = R"_(import Score
import "helper.js" as H
Script {
  FloatSlider { objectName: H.name; min: 0; max: 10; init: 1 }
  tick: function(token, state) { }
})_";
constexpr auto fileScriptLater = R"_(import Score
import "helper.js" as H
Script {
  FloatSlider { objectName: H.name; min: 0; max: 10; init: 1 }
  FloatSlider { objectName: "more"; min: 0; max: 10; init: 1 }
  tick: function(token, state) { }
})_";
}

TEST_CASE(
    "a Javascript preset carries the state of its script with its controls",
    "[integration][js][preset]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto& js = addScript(*doc);
    level(js).setValue(ossia::value{4.f});
    js.setState(stateA);
    const auto preset = save(js);

    level(js).setValue(ossia::value{9.f});
    js.setState(stateB);
    load(js, preset);
    REQUIRE(js.state() == stateA);
    REQUIRE(level(js).value() == ossia::value{4.f});
  });
}

TEST_CASE(
    "a Javascript preset with an empty state clears the state of the script",
    "[integration][js][preset]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto& js = addScript(*doc);
    const auto empty = save(js);
    js.setState(stateA);
    load(js, empty);
    REQUIRE(js.state().empty());
  });
}

TEST_CASE(
    "a Javascript preset written before the state was part of it still loads",
    "[integration][js][preset]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto& js = addScript(*doc);
    js.setState(stateA);

    auto preset = save(js);
    preset.data
        = QStringLiteral("[[%1,{\"Float\":6.0}]]").arg(level(js).id().val()).toUtf8();
    load(js, preset);
    REQUIRE(level(js).value() == ossia::value{6.f});
    // A preset without a state leaves the state unchanged
    REQUIRE(js.state() == stateA);
  });
}

TEST_CASE(
    "a Javascript preset keeps its state through its file form",
    "[integration][js][preset]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto& js = addScript(*doc);
    js.setState(stateA);
    const auto json = save(js).toJson();

    js.setState(stateB);
    auto preset
        = Process::Preset::fromJson(ctx.interfaces<Process::ProcessFactoryList>(), json);
    REQUIRE(preset);
    load(js, *preset);
    REQUIRE(js.state() == stateA);
  });
}

TEST_CASE(
    "loading a Javascript preset is undone with the state it replaced",
    "[integration][js][preset]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    const auto& dctx = doc->context();
    auto& js = addScript(*doc);
    js.setState(stateA);
    const auto preset = save(js);
    js.setState(stateB);

    auto& factories = ctx.interfaces<Process::LoadPresetCommandFactoryList>();
    auto cmd
        = factories.make(&Process::LoadPresetCommandFactory::make, js, preset, dctx);
    REQUIRE(cmd);
    CommandDispatcher<>{dctx.commandStack}.submit(cmd);
    REQUIRE(js.state() == stateA);

    doc->commandStack().undo();
    REQUIRE(js.state() == stateB);
    doc->commandStack().redo();
    REQUIRE(js.state() == stateA);
  });
}

TEST_CASE(
    "a Javascript preset of another script brings that script, undone with the one it replaced",
    "[integration][js][preset]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto& js = addScript(*doc);
    level(js).setValue(ossia::value{4.f});
    js.setState(stateA);

    auto& other = addScript(*doc);
    (void)other.setProgram(JS::QmlSource{otherScript, otherUi});
    REQUIRE(other.inlets().size() == 2);
    qobject_cast<Process::ControlInlet*>(other.inlets()[1])->setValue(ossia::value{0.25f});
    other.setState(stateB);
    const auto preset = save(other);

    loadAsCommand(ctx, *doc, js, preset);
    REQUIRE(js.program() == other.program());
    REQUIRE(js.inlets().size() == 2);
    REQUIRE(
        qobject_cast<Process::ControlInlet*>(js.inlets()[1])->value()
        == ossia::value{0.25f});
    REQUIRE(js.state() == stateB);

    doc->commandStack().undo();
    REQUIRE(js.program() == JS::QmlSource{script, {}});
    REQUIRE(js.inlets().size() == 1);
    REQUIRE(level(js).value() == ossia::value{4.f});
    REQUIRE(js.state() == stateA);
  });
}

TEST_CASE(
    "a Javascript preset of a script from a file holds the file, and loads what it holds then",
    "[integration][js][preset]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    const auto path = dir.filePath("Lib.qml");
    write(dir.filePath("helper.js"), helper);
    write(path, fileScript);

    auto doc = score::test::new_document(ctx);
    auto& fromFile = *qobject_cast<JS::ProcessModel*>(
        score::test::add_process(*doc, js_uuid, path));
    REQUIRE(fromFile.followsRootFile());
    REQUIRE(fromFile.inlets().size() == 1);
    REQUIRE(fromFile.inlets()[0]->name() == QStringLiteral("fromhelper"));

    const auto preset = save(fromFile);
    REQUIRE(preset.key.effect == path);
    const auto data = readJson(preset.data);
    REQUIRE(data.HasMember("Root"));
    REQUIRE(!data.HasMember("Script"));
    REQUIRE(matches(fromFile, preset));

    // After the file changes, loading the preset uses the current file content and includes
    write(path, fileScriptLater);
    auto& js = addScript(*doc);
    loadAsCommand(ctx, *doc, js, preset);
    REQUIRE(js.rootFile() == path);
    REQUIRE(js.followsRootFile());
    REQUIRE(js.inlets().size() == 2);
    REQUIRE(js.inlets()[0]->name() == QStringLiteral("fromhelper"));
    REQUIRE(matches(js, preset));

    // A preset keyed by the script text matches too
    auto old = preset;
    old.key.effect = QString::fromUtf8(js.qmlData());
    REQUIRE(matches(js, old));

    doc->commandStack().undo();
    REQUIRE(js.rootFile().isEmpty());
    REQUIRE(js.program() == JS::QmlSource{script, {}});
  });
}

TEST_CASE(
    "a Javascript preset of a script edited away from its file holds both",
    "[integration][js][preset]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    const auto path = dir.filePath("Lib.qml");
    write(dir.filePath("helper.js"), helper);
    write(path, fileScript);

    auto doc = score::test::new_document(ctx);
    auto& fromFile = *qobject_cast<JS::ProcessModel*>(
        score::test::add_process(*doc, js_uuid, path));
    (void)fromFile.setProgram(JS::QmlSource{fileScriptLater, {}});
    REQUIRE(!fromFile.followsRootFile());
    const auto preset = save(fromFile);
    const auto data = readJson(preset.data);
    REQUIRE(data.HasMember("Root"));
    REQUIRE(data.HasMember("Script"));

    // Includes are resolved relative to the file
    auto& js = addScript(*doc);
    loadAsCommand(ctx, *doc, js, preset);
    REQUIRE(js.rootFile() == path);
    REQUIRE(!js.followsRootFile());
    REQUIRE(js.inlets().size() == 2);
    REQUIRE(js.inlets()[0]->name() == QStringLiteral("fromhelper"));
  });
}
