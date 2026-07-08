// Integration test for the JS scripting API: creating a Javascript process
// parses its (default) QML script through the QML engine and exposes the
// inlets/outlets it declares (a ValueInlet + ValueOutlet). This exercises the
// script -> process-model binding without running execution.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QPointF>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("A Javascript process exposes ports from its script", "[integration][js][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();

    // The Javascript process factory.
    auto& factories = ctx.interfaces<Process::ProcessFactoryList>();
    const auto js_key = UuidKey<Process::ProcessModel>::fromString(
        QStringLiteral("846a5de5-47f9-46c5-a898-013cb20951d0"));
    auto* factory = factories.get(js_key);
    REQUIRE(factory != nullptr);

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
        interval, factory->concreteKey(), factory->customConstructionData(), QPointF{});

    // Locate the JS process we just added.
    Process::ProcessModel* js = nullptr;
    for(auto& p : interval.processes)
    {
      if(p.concreteKey() == js_key)
        js = &p;
    }
    REQUIRE(js != nullptr);

    // Its default script declares a ValueInlet and a ValueOutlet.
    CHECK(js->inlets().size() >= 1);
    CHECK(js->outlets().size() >= 1);
  });
}
