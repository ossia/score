// The execution's edit commands: the callbacks the audio thread queues for the
// UI thread, run by Execution::DocumentPlugin from a timer.

#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Process/ScenarioExecution.hpp>

#include <Execution/DocumentPlugin.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/model/ComponentUtils.hpp>

#include <core/command/CommandStack.hpp>

#include <ossia/detail/thread.hpp>
#include <ossia/editor/scenario/time_interval.hpp>

#include <QApplication>
#include <QElapsedTimer>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Process.hpp>

#include <stdexcept>

namespace
{
QStringList g_messages;
void captureMessages(QtMsgType, const QMessageLogContext&, const QString& msg)
{
  g_messages.push_back(msg);
}

//! Edit commands are queued by the audio thread: the test thread poses as one.
template <typename F>
void enqueueEdit(Execution::DocumentPlugin& plug, F&& f)
{
  ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
  plug.contextData()->m_editionQueue.enqueue(std::forward<F>(f));
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
}

template <typename F>
bool spinUntil(F&& done, int ms = 2000)
{
  QElapsedTimer t;
  t.start();
  while(!done() && t.elapsed() < ms)
    QApplication::processEvents(QEventLoop::AllEvents, 10);
  return done();
}
}

TEST_CASE(
    "An edit command that throws is reported and the commands after it still run",
    "[unit][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(false, score::test::base_interval(*doc));
    score::test::run_exec(plug);

    int ran = 0;
    g_messages.clear();
    auto previous = qInstallMessageHandler(captureMessages);
    enqueueEdit(plug, [] { throw std::runtime_error("edit command failure"); });
    enqueueEdit(plug, [&ran] { ran++; });
    // The DocumentPlugin's timer runs them
    const bool first = spinUntil([&] { return ran == 1; });
    enqueueEdit(plug, [&ran] { ran++; });
    const bool second = spinUntil([&] { return ran == 2; });
    qInstallMessageHandler(previous);

    CHECK(first);
    CHECK(second);
    CHECK(g_messages.filter("edit command failure").size() == 1);
  });
}

TEST_CASE(
    "A graph interval's execution callback arriving after the interval was removed",
    "[unit][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    CommandDispatcher<> disp{doc->context().commandStack};
    auto& scenar = score::test::base_scenario(*doc);
    auto& start = scenar.states.at(scenar.startEvent().states().front());
    auto create = new Scenario::Command::CreateInterval_State_Event_TimeSync{
        scenar, start.id(), TimeVal::fromMsecs(2000), 0.5, true};
    disp.submit(create);
    const auto itvId = create->createdInterval();
    REQUIRE(scenar.intervals.at(itvId).graphal());

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(false, score::test::base_interval(*doc));
    score::test::run_exec(plug);

    auto comp = score::findComponent<Execution::ScenarioComponent>(scenar.components());
    REQUIRE(comp);
    std::shared_ptr<ossia::time_interval> itv
        = comp->intervals().at(itvId)->OSSIAInterval();
    REQUIRE(itv);
    REQUIRE(itv->graphal);

    // The audio thread reports the interval's state: an edit command is queued
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    itv->stop();
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

    // The interval is removed before the UI thread runs that command; its
    // component outlives it until the audio thread runs the cleanup.
    doc->commandStack().undo();
    REQUIRE(scenar.intervals.find(itvId) == scenar.intervals.end());

    // Run the command as the timer would, but without its guard
    int count = 0;
    Execution::ExecutionCommand cmd;
    while(plug.contextData()->m_editionQueue.try_dequeue(cmd))
    {
      CHECK_NOTHROW(cmd());
      count++;
    }
    CHECK(count > 0);

    score::test::run_exec(plug);
  });
}
