// Timing of the state and automation edits on a process with many controls
// (fomo), as done by hand: snapshot the process in the states of an
// interval, interpolate them into automations, and resize the interval with
// the mouse (one ongoing MoveEventMeta updated at every mouse move).
#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/State/MessageNode.hpp>

#include <Automation/AutomationModel.hpp>
#include <Scenario/Commands/Cohesion/InterpolateStates.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>
#include <Scenario/Commands/State/SnapshotProcess.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>

#include <QElapsedTimer>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Events.hpp>
#include <score_test/Process.hpp>
#include <score_test/Project.hpp>

#include <catch2/catch_all.hpp>

namespace
{
// Budgets for a normal (debug or release) build. Sanitizers slow everything
// several times over: there the budgets are not checked.
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
constexpr bool check_budgets = false;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
constexpr bool check_budgets = false;
#else
constexpr bool check_budgets = true;
#endif
#else
constexpr bool check_budgets = true;
#endif

const QString fomo_uuid = QStringLiteral("e9f0a1b2-3c4d-5e6f-7a8b-9c0d1e2f3a4b");

using score::test::settle;

template <typename F>
qint64 timed(F&& f)
{
  QElapsedTimer t;
  t.start();
  f();
  settle(5);
  return t.elapsed();
}
}

TEST_CASE(
    "snapshots, interpolation and resizing stay interactive with many controls",
    "[integration][automation][bench]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    auto& ctx = doc->context();
    auto proc = score::test::add_process(*doc, fomo_uuid, {});
    if(!proc)
      SKIP("score-addon-synthimi is not built");
    CommandDispatcher<>{ctx.commandStack}.submit<Process::RenameProcess>(
        *proc, QStringLiteral("fomo"));
    auto& scenar = score::test::base_scenario(*doc);

    auto create = new Scenario::Command::CreateInterval_State_Event_TimeSync{
        scenar, scenar.states.begin()->id(), TimeVal::fromMsecs(2000), 0.3, false};
    const auto itvId = create->createdInterval();
    CommandDispatcher<>{ctx.commandStack}.submit(create);
    auto& itv = scenar.interval(itvId);
    auto& start = scenar.state(itv.startState());
    auto& end = scenar.state(itv.endState());

    const auto snap1
        = timed([&] { Scenario::Command::snapshotProcessInState(start, *proc, ctx); });
    CHECK(!Process::flatten(start.messages().rootNode()).empty());

    // Change every numeric control before the second snapshot
    for(auto in : proc->inlets())
      if(auto c = qobject_cast<Process::ControlInlet*>(in))
        if(auto f = c->value().target<float>())
          c->setValue(*f * 0.5f + 0.1f);
    const auto snap2
        = timed([&] { Scenario::Command::snapshotProcessInState(end, *proc, ctx); });

    const auto interp = timed([&] {
      Scenario::Command::InterpolateStates({&itv}, ctx.commandStack);
    });
    int automations = 0;
    for(auto& p : itv.processes)
      if(qobject_cast<Automation::ProcessModel*>(&p))
        automations++;
    CHECK(automations > 10);

    auto& endEvent = scenar.event(end.eventId());
    const auto resize = timed([&] {
      SingleOngoingCommandDispatcher<Scenario::Command::MoveEventMeta> disp{ctx.commandStack};
      for(int i = 0; i < 50; i++)
      {
        disp.submit(
            scenar, endEvent.id(), TimeVal::fromMsecs(2000 + 40 * i), 0.3,
            ExpandMode::Scale, LockMode::Free);
        settle();
      }
      disp.commit();
    });
    CHECK(scenar.timeSync(endEvent.timeSync()).date() == TimeVal::fromMsecs(2000 + 40 * 49));

    const auto snap3 = timed([&] {
      Scenario::Command::snapshotProcessInState(end, *proc, ctx);
    });

    // An edit by hand must not take seconds, even in a debug build.
    INFO("ms: snapshots " << snap1 << ", " << snap2 << ", " << snap3 << "; interpolation "
                          << interp << "; resize " << resize);
    if(check_budgets)
    {
      CHECK(snap1 < 1000);
      CHECK(snap2 < 1000);
      CHECK(snap3 < 1000);
      CHECK(interp < 2000);
      CHECK(resize < 2000);
    }
  });
}
