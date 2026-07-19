// Regression test for the re-entrant exit abort: Presenter::exit() pumps the
// event loop while closing documents (execution stop, deferred deletions), so
// a second exit request arriving meanwhile — an OSC /exit callback firing
// again, or an already-queued quit timer — re-entered closeAllDocuments and
// tore down half-destroyed documents, aborting the process.
//
// Presenter::exit() now latches: the first call closes the documents, any
// further call while exiting (or after a completed exit) is a no-op returning
// false. A cancelled close (user said no to a save prompt) resets the latch so
// the session can exit later. GUIApplicationInterface::requestExit() also
// tolerates the presenter being gone.

#include <score_test/App.hpp>

#include <core/application/MinimalApplication.hpp>
#include <core/presenter/Presenter.hpp>

#include <QApplication>

#include <catch2/catch_test_macros.hpp>

TEST_CASE(
    "A second exit request during or after shutdown is a no-op",
    "[integration][regression][teardown]")
{
  score::test::prepare_test_environment(/*headless=*/true);
  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");

  static int argc = 1;
  static char arg0[] = "score-test";
  static char* argv[] = {arg0, nullptr};
  score::MinimalGUIApplication app{argc, argv, /*show=*/false};
  QApplication::processEvents();

  auto* pres = qApp->findChild<score::Presenter*>();
  REQUIRE(pres);

  // First request closes every document and reports success.
  CHECK(pres->exit() == true);

  // Any further request is latched out instead of re-entering the close.
  CHECK(pres->exit() == false);
  CHECK(pres->exit() == false);

  // The full request path stays safe too (this is what the OSC /exit
  // callback and the quit timer end up calling).
  app.requestExit();
  app.forceExit();
  QApplication::processEvents();
}
