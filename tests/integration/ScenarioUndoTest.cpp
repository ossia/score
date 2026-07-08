// Integration test for the command / undo-redo API on the scenario model.
// Adds a process to the document's base interval, then checks that undo and
// redo restore the process count exactly.
//
// This uses run_in_gui_app (GUI stack, window hidden): undoing removes the
// process, which prunes the selection through the document presenter — only
// valid when the GUI stack exists. Needs a display (X11 / Xvfb).

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/ProcessList.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QPointF>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Adding a process to the base interval is undoable", "[integration][command][undo][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& scenario_dm
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate());
    auto& interval = scenario_dm.baseInterval();

    // A specific, dependency-light core process (Automation); an arbitrary
    // factory may require construction data and crash when instantiated blank.
    auto& factories = ctx.interfaces<Process::ProcessFactoryList>();
    auto* factory = factories.get(UuidKey<Process::ProcessModel>::fromString(
        QStringLiteral("d2a67bd8-5d3f-404e-b6e9-e350cf2a833f")));
    REQUIRE(factory != nullptr);

    const auto initial = interval.processes.size();

    score::CommandStack& stack = doc->commandStack();
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
        interval, factory->concreteKey(), factory->customConstructionData(), QPointF{});

    CHECK(interval.processes.size() == initial + 1);

    REQUIRE(stack.canUndo());
    stack.undo();
    CHECK(interval.processes.size() == initial);

    REQUIRE(stack.canRedo());
    stack.redo();
    CHECK(interval.processes.size() == initial + 1);
  });
}
