// Regression test for 99d0cfe82 — "core: drain the event queue before
// deleting the presenter".
//
// Both Minimal[GUI]Application destructors ran QApplication::processEvents()
// AFTER `delete m_presenter`, so deferred slots queued during the app's
// lifetime (e.g. Scenario::SearchWidget's deferred init reaching into the
// device explorer) were dispatched against the freed presenter-owned
// application context — a use-after-free at shutdown. The fix drains the
// queue while the presenter is still alive.
//
// We queue a slot that reads the presenter-owned GUIApplicationContext and is
// still pending when the application is destroyed (the receiver context is the
// QApplication, which outlives the presenter). Without the fix, ASAN reports a
// heap-use-after-free when the destructor dispatches it; with the fix the slot
// runs before the presenter is deleted and reads valid memory.

#include <score_test/App.hpp>

#include <core/application/MinimalApplication.hpp>

#include <QApplication>

#include <catch2/catch_test_macros.hpp>

TEST_CASE(
    "Deferred slots reading the application context are drained before the "
    "presenter dies",
    "[integration][regression][teardown]")
{
  score::test::prepare_test_environment(/*headless=*/true);
  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");

  bool slot_ran = false;
  bool gui_flag = false;

  {
    score::MinimalApplication app;
    QApplication::processEvents();
    QApplication::processEvents();

    // The context object is owned by the (heap-allocated) presenter.
    const score::GUIApplicationContext* ctx = &app.context();

    // Queue a deferred slot AFTER all processEvents calls, so it is still
    // pending when ~MinimalApplication runs. Receiver context: the
    // QApplication, which outlives the presenter — exactly like the deferred
    // plugin-load slots of the original crash.
    QMetaObject::invokeMethod(
        app.m_app.get(),
        [&slot_ran, &gui_flag, ctx] {
      // Reads presenter-owned heap memory: a use-after-free if dispatched
      // after `delete m_presenter`.
      gui_flag = ctx->applicationSettings.gui;
      slot_ran = true;
        },
        Qt::QueuedConnection);

    score::test::close_all_documents(app.context());
    // NOTE: close_all_documents processes events, which would dispatch our
    // slot early — so queue it again afterwards to be certain one instance
    // is pending at destruction time.
    slot_ran = false;
    QMetaObject::invokeMethod(
        app.m_app.get(),
        [&slot_ran, &gui_flag, ctx] {
      gui_flag = ctx->applicationSettings.gui;
      slot_ran = true;
        },
        Qt::QueuedConnection);
    // ~MinimalApplication runs here: with the fix it drains the queue while
    // the presenter is alive; without it, the slot fires on freed memory.
  }

  CHECK(slot_ran);
  CHECK(gui_flag);
}
