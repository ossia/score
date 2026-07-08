// Integration test: creating a time-sync (+ its event and state) in the
// scenario is undoable. Creation selects the new elements, which routes through
// the document presenter, so this runs in GUI mode.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Scenario/Commands/Scenario/Creations/CreateTimeSync_Event_State.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Creating a time-sync in the scenario is undoable", "[integration][command][undo][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();

    // The base interval's default process is the top-level scenario.
    REQUIRE(interval.processes.begin() != interval.processes.end());
    auto& scenario
        = static_cast<Scenario::ProcessModel&>(*interval.processes.begin());

    const auto ts0 = scenario.timeSyncs.size();
    const auto ev0 = scenario.events.size();
    const auto st0 = scenario.states.size();

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Scenario::Command::CreateTimeSync_Event_State>(
        scenario, TimeVal::fromMsecs(1000.), 0.5);

    CHECK(scenario.timeSyncs.size() == ts0 + 1);
    CHECK(scenario.events.size() == ev0 + 1);
    CHECK(scenario.states.size() == st0 + 1);

    score::CommandStack& stack = doc->commandStack();
    REQUIRE(stack.canUndo());
    stack.undo();
    CHECK(scenario.timeSyncs.size() == ts0);
    CHECK(scenario.events.size() == ev0);
    CHECK(scenario.states.size() == st0);

    stack.redo();
    CHECK(scenario.timeSyncs.size() == ts0 + 1);
  });
}
