// The custom UI half of the Javascript process: JS/Qml/QmlObjects.hpp's
// ScriptUI, ComponentCache::getUi and ProcessModel::createItemForUI, all
// running on the ApplicationPlugin's m_scriptProcessUIEngine.
//
// A Javascript process carries two scripts, "Execution" and "UI", and the UI
// one is arbitrary QML out of a .score file. Loading it must report an error
// rather than crash, and above all it must not leave the process claiming a UI
// it cannot build: Process::ProcessFlags::ExternalEffect is what puts the
// button in the inspector, and hasExternalUI() is what Score.hasProcessUI()
// answers.
//
// Nothing else in the tree exercises the UI script at all: JsProcessTest only
// checks that the default execution script exposes ports.
//
// One TEST_CASE, deliberately. JS::ProcessModel::rootPath() caches the
// Library settings model in a function-local `static const auto&`
// (JSProcessModel.cpp:170), so the second score application built in a process
// reads the first one's freed settings. See JsRootPathStaticTest.cpp: until
// that is fixed, a JS process may only be created under the first
// run_in_gui_app of a binary.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Effect/EffectLayer.hpp>
#include <JS/JSProcessModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QPointF>
#include <QQuickItem>

#include <catch2/catch_test_macros.hpp>

namespace
{
const char* trivial_execution = R"_(import Score
Script {
  ValueOutlet { id: out; objectName: "out" }
  tick: function(token, state) { }
})_";

const char* valid_ui = R"_(import Score
import QtQuick
ScriptUI {
  width: 100
  height: 50
  Rectangle { anchors.fill: parent; color: "red" }
})_";

JS::ProcessModel& addJsProcess(score::Document& doc, const score::ApplicationContext& app)
{
  auto& interval
      = static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate())
            .baseInterval();

  auto& factories = app.interfaces<Process::ProcessFactoryList>();
  const auto js_key = UuidKey<Process::ProcessModel>::fromString(
      QStringLiteral("846a5de5-47f9-46c5-a898-013cb20951d0"));
  auto* factory = factories.get(js_key);
  REQUIRE(factory != nullptr);

  CommandDispatcher<> disp{doc.context().commandStack};
  disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
      interval, factory->concreteKey(), factory->customConstructionData(), QPointF{});

  JS::ProcessModel* js = nullptr;
  for(auto& p : interval.processes)
    if(auto* j = qobject_cast<JS::ProcessModel*>(&p))
      js = j;
  REQUIRE(js != nullptr);
  return *js;
}
}

TEST_CASE("The UI script of a Javascript process", "[integration][js][gui][ui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& js = addJsProcess(*doc, ctx);

    // JS::ProcessModel's own signals (errorMessage, uiScriptOk) cannot be
    // connected from here: the test binary links score_plugin_js while the
    // application loads plugins/libscore_plugin_js.so, so the class has two
    // metaobjects and QObject::connect on a pointer-to-member returns an
    // invalid Connection -- measured, both connects return false. Signals
    // inherited from Process::ProcessModel (score_lib_process, one copy) do
    // connect. So the error paths below are asserted on state, not signals.
    int flags_changed = 0;
    QObject::connect(
        &js, &Process::ProcessModel::flagsChanged, &js, [&] { flags_changed++; });

    // ---- no UI script at all ----------------------------------------------
    REQUIRE(js.setProgram(JS::QmlSource{trivial_execution, {}}).valid);
    CHECK_FALSE(js.hasExternalUI());
    CHECK(js.createItemForUI(doc->context()) == nullptr);
    // Showing a UI that does not exist is a no-op, not a crash.
    Process::setupExternalUI(js, doc->context(), true);
    Process::setupExternalUI(js, doc->context(), false);

    // ---- a UI script that loads -------------------------------------------
    REQUIRE(js.setProgram(JS::QmlSource{trivial_execution, valid_ui}).valid);
    CHECK(js.hasExternalUI());
    CHECK(flags_changed == 1);
    {
      auto* item = js.createItemForUI(doc->context());
      REQUIRE(item != nullptr);
      CHECK(item->width() == 100.);
      CHECK(item->height() == 50.);
      delete item;
    }

    // ---- a UI script that does not parse ----------------------------------
    REQUIRE(js.setProgram(JS::QmlSource{trivial_execution, "import Score\nScriptUI { ((("})
                .valid);
    CHECK_FALSE(js.hasExternalUI());
    CHECK(js.createItemForUI(doc->context()) == nullptr);
    CHECK(flags_changed == 2);

    // ---- a UI script whose root is not a ScriptUI -------------------------
    REQUIRE(
        js.setProgram(JS::QmlSource{trivial_execution, "import QtQuick\nItem { }"}).valid);
    CHECK_FALSE(js.hasExternalUI());
    CHECK(js.createItemForUI(doc->context()) == nullptr);

    // ---- a UI script importing a module that does not exist ---------------
    REQUIRE(js.setProgram(
                  JS::QmlSource{trivial_execution, "import NoSuchModule 9.9\nScriptUI { }"})
                .valid);
    CHECK_FALSE(js.hasExternalUI());

    // ---- the execution script is untouched by any of it --------------------
    CHECK(js.executionScript() == QString::fromUtf8(trivial_execution));
    CHECK(js.inlets().size() + js.outlets().size() >= 1);
  });
}
