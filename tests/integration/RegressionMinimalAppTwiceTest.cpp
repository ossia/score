// Regression test for 43fc99280 — "core: make MinimalApplication default
// arguments static".
//
// MinimalApplication's default constructor delegated to the (argc, argv)
// constructor using its OWN data members as arguments, reading them before the
// object's lifetime had begun; additionally QApplication keeps a reference to
// argc for its whole lifetime, so the storage must outlive it. The fix makes
// the default arguments static.
//
// This test default-constructs and destroys MinimalApplication twice in one
// process: the default-argument storage must stay valid across both app
// lifetimes (and QApplication's held argc reference must never dangle).

#include <score_test/App.hpp>

#include <core/application/MinimalApplication.hpp>

#include <QApplication>

#include <catch2/catch_test_macros.hpp>

TEST_CASE(
    "MinimalApplication can be default-constructed and destroyed twice",
    "[integration][regression][application]")
{
  score::test::prepare_test_environment(/*headless=*/true);

  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");

  for(int i = 0; i < 2; ++i)
  {
    INFO("iteration " << i);
    {
      // The default constructor is the fixed code path: each instance gets
      // its own argc/argv, so the previous QApplication cannot have eaten
      // them (it edits argc/argv in place).
      score::MinimalApplication app;

      REQUIRE(qApp != nullptr);
      // The second application must see a full command line, not the
      // leftovers of the first one.
      CHECK(QCoreApplication::arguments().size() == 1);
      CHECK(!QCoreApplication::arguments().front().isEmpty());

      QApplication::processEvents();
      QApplication::processEvents();

      score::test::close_all_documents(app.context());
      QApplication::processEvents();
    }
    // The application must be fully torn down before the next round.
    REQUIRE(qApp == nullptr);
  }

  SUCCEED("two full MinimalApplication lifecycles completed");
}
