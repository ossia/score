// App test: the reset that follows a stop, when documents change under it.
//
// ExecutionController::reset_after_stop() arms a timer to reset the stopped
// document's base interval a little later (so that the last messages get
// out). The timer used to act on whichever document was current when it
// fired: the wrong document after a switch, and a crash in
// BaseScenarioContainer::interval() if another document was being closed at
// that moment (its model already torn down, its processEvents() running the
// timer) - one in two closes of the current document hit it. The reset now
// acts on the interval it captured.
//
// One app per case: run the cases separately on Windows.

#include <Engine/ApplicationPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QApplication>
#include <QElapsedTimer>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

namespace
{
void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
    QApplication::processEvents(QEventLoop::AllEvents, 10);
}

Scenario::IntervalModel& baseInterval(score::Document& doc)
{
  return score::IDocument::get<Scenario::ScenarioDocumentModel>(doc).baseInterval();
}

// Counts the Finished events the reset sends to an interval
struct FinishedCounter
{
  int count{};
  explicit FinishedCounter(Scenario::IntervalModel& itv)
  {
    QObject::connect(
        &itv, &Scenario::IntervalModel::executionEvent, &itv,
        [this](Scenario::IntervalExecutionEvent ev) {
      if(ev == Scenario::IntervalExecutionEvent::Finished)
        count++;
        });
  }
};
}

TEST_CASE("The reset after a stop goes to the document that was stopped", "[execution][documents]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& exec = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>().execution();

    auto a = score::test::new_document(ctx);
    FinishedCounter finishedA{baseInterval(*a)};
    // Stopping (even when not playing) arms the delayed reset of a's base interval
    exec.request_stop();
    spin(20);

    // Another document is current when the timer fires
    auto b = score::test::new_document(ctx);
    REQUIRE(ctx.docManager.currentDocument() == b);
    spin(150);

    // Opening b requested a stop too (prepareNewDocument), so more than one
    // reset may have gone to a: what matters is that none went astray to b
    // instead. Before the fix a got none.
    CHECK(finishedA.count >= 1);
  });
}

TEST_CASE("The reset after a stop survives another document closing", "[execution][documents]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& exec = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>().execution();

    auto a = score::test::new_document(ctx);
    exec.request_stop();
    spin(20);

    // b becomes current and is closed right away: forceCloseDocument() tears
    // its model down, then runs the event loop - where the timer may fire.
    auto b = score::test::new_document(ctx);
    ctx.docManager.forceCloseDocument(ctx, *b);
    spin(150);

    CHECK(ctx.docManager.currentDocument() == a);
    CHECK(!score::IDocument::get<Scenario::ScenarioDocumentModel>(*a).closing());
  });
}

TEST_CASE("The reset after a stop is dropped with its document", "[execution][documents]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& exec = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>().execution();

    auto a = score::test::new_document(ctx);
    auto b = score::test::new_document(ctx);
    exec.request_stop();
    spin(20);
    ctx.docManager.forceCloseDocument(ctx, *b);
    spin(150);

    CHECK(ctx.docManager.currentDocument() == a);
  });
}
