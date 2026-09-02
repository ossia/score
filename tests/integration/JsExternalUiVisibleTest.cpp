// Opening the custom UI of a Javascript process tells nothing that it is open.
//
// Process::setupExternalUI (EffectLayer.cpp:165) builds and shows the window
// but emits nothing itself; every emitter of
// Process::ProcessModel::externalUIVisible lives in the plug-ins. LV2, VST and
// VST3 windows emit `true` from their show path (LV2/Window.cpp:280,
// Vst/Window.cpp:50, Vst3/UI/WindowCommon.cpp:21); the Javascript process now
// emits `true` from the end of createWindowForUI too (it previously only
// emitted `false` on close: JSProcessModel.cpp:407, :444, :454, :655).
//
// What reads that signal is the state of the two controls that open the UI:
// the process-header button (EffectLayer.cpp:233) and the inspector toggle
// (ProcessInspectorWidgetDelegateFactory.cpp:211 and :241). With no `true`
// they stay in the closed state while the window is open, so the next click
// tries to open it again instead of closing it.
//
// Measured: three show/hide pairs produce three signals, all of them false.
//
// Its own executable: JS::ProcessModel::rootPath() caches the Library settings
// in a function-local static, so only the first application of a process may
// create a Javascript process. See JsRootPathStaticTest.cpp.

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

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Showing a Javascript process UI reports that it is visible",
          "[integration][js][gui][ui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    auto& factories = ctx.interfaces<Process::ProcessFactoryList>();
    const auto js_key = UuidKey<Process::ProcessModel>::fromString(
        QStringLiteral("846a5de5-47f9-46c5-a898-013cb20951d0"));
    auto* factory = factories.get(js_key);
    REQUIRE(factory != nullptr);

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
        interval, factory->concreteKey(), factory->customConstructionData(), QPointF{});

    JS::ProcessModel* js = nullptr;
    for(auto& p : interval.processes)
      if(auto* j = qobject_cast<JS::ProcessModel*>(&p))
        js = j;
    REQUIRE(js != nullptr);

    REQUIRE(js->setProgram(
                   JS::QmlSource{
                       R"_(import Score
Script {
  ValueOutlet { id: out; objectName: "out" }
  tick: function(token, state) { }
})_",
                       R"_(import Score
import QtQuick
ScriptUI {
  width: 100
  height: 50
  Rectangle { anchors.fill: parent; color: "red" }
})_"})
                .valid);
    REQUIRE(js->hasExternalUI());

    std::vector<bool> visible;
    QObject::connect(
        js, &Process::ProcessModel::externalUIVisible, js,
        [&](bool v) { visible.push_back(v); });

    for(int i = 0; i < 3; i++)
    {
      Process::setupExternalUI(*js, doc->context(), true);
      REQUIRE(js->externalUI != nullptr);
      Process::setupExternalUI(*js, doc->context(), false);
      CHECK(js->externalUI == nullptr);
    }

    REQUIRE(visible.size() == 6);
    CHECK(visible[0] == true);
    CHECK(visible[1] == false);
  });
}
