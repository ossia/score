// Tearing down the base scenario must not clean its time syncs up on the main
// thread while their cleanup is still queued for the audio thread, or both
// threads clear the same callbacks concurrently.
#include <Scenario/Document/TimeSync/TimeSyncExecution.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <core/document/Document.hpp>

#include <ossia/editor/scenario/time_sync.hpp>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Project.hpp>

TEST_CASE(
    "The base scenario outlives the audio thread's cleanup of its time syncs",
    "[unit][execution][teardown]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(false, score::test::base_interval(*doc));

    auto drain_gc = [&] {
      Execution::GCCommand gc;
      while(plug.contextData()->m_gcQueue.try_dequeue(gc))
        gc();
    };
    score::test::run_exec(plug);

    auto base = plug.baseScenario();
    REQUIRE(base);
    const std::shared_ptr<ossia::time_sync> ts
        = base->startTimeSync().OSSIATimeSync();
    REQUIRE(ts);
    REQUIRE_FALSE(ts->callbacks.callbacks.empty());

    base->cleanup();
    // The audio thread has not run the queued cleanup yet: nothing on this
    // thread may have touched the time sync's callbacks.
    CHECK_FALSE(ts->callbacks.callbacks.empty());

    score::test::run_exec(plug);
    drain_gc();
    CHECK(ts->callbacks.callbacks.empty());
  });
}
