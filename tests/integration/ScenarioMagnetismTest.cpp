// A scenario attracts what is dragged in a sibling process (an automation
// point in the same interval, for instance) to its time syncs, where its
// states are.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Scenario/Commands/Scenario/Creations/CreateTimeSync_Event_State.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/document/Document.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("A scenario's time syncs attract what moves in its siblings", "[integration][magnetism]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& interval = score::test::base_interval(*doc);
    REQUIRE(interval.processes.begin() != interval.processes.end());
    auto& scenario = static_cast<Scenario::ProcessModel&>(*interval.processes.begin());

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Scenario::Command::CreateTimeSync_Event_State>(
        scenario, TimeVal::fromMsecs(1000.), 0.3);
    disp.submit<Scenario::Command::CreateTimeSync_Event_State>(
        scenario, TimeVal::fromMsecs(3000.), 0.6);

    auto at = [&](const QObject* o, double ms) {
      const Process::ProcessModel& proc = scenario;
      auto pos = proc.magneticPosition(o, TimeVal::fromMsecs(ms));
      REQUIRE(pos);
      CHECK(pos->snapLine);
      return pos->time;
    };
    CHECK(at(nullptr, 1100.) == TimeVal::fromMsecs(1000.));
    CHECK(at(nullptr, 2900.) == TimeVal::fromMsecs(3000.));
    CHECK(at(nullptr, 400.) == TimeVal::zero());

    // A time sync being moved is not attracted to itself.
    const Scenario::TimeSyncModel* moved{};
    for(const auto& ts : scenario.timeSyncs)
      if(ts.date() == TimeVal::fromMsecs(3000.))
        moved = &ts;
    REQUIRE(moved);
    CHECK(at(moved, 2900.) == TimeVal::fromMsecs(1000.));

    // The tools moving a sync or a brace, and those creating elements, ask with
    // the scenario itself: it does not attract its own elements, which would
    // snap a slow drag back to where it started.
    const Process::ProcessModel& proc = scenario;
    CHECK_FALSE(proc.magneticPosition(&scenario, TimeVal::fromMsecs(2900.)));
  });
}
