#include <score/tools/Timers.hpp>

#include <QCoreApplication>

#include <catch2/catch_test_macros.hpp>

#include <limits>

TEST_CASE("a rate given as a period shares the timer of the same rate in Hz", "[timers]")
{
  int argc = 1;
  char arg0[] = "test";
  char* argv[] = {arg0, nullptr};
  QCoreApplication* app
      = QCoreApplication::instance() ? nullptr : new QCoreApplication(argc, argv);
  {
    score::Timers timers;
    QObject a, b;

    const double hz = 60.;
    const double fromPeriod = 1000. / (1000. / hz);
    REQUIRE(fromPeriod != hz);

    auto* ta = timers.acquireTimer(&a, hz);
    auto* tb = timers.acquireTimer(&b, fromPeriod);
    REQUIRE(ta == tb);

    auto* tc = timers.acquireTimer(&b, 50.);
    REQUIRE(tc != ta);

    timers.releaseTimer(&a, ta);
    timers.releaseTimer(&b, tb);
    timers.releaseTimer(&b, tc);
  }
  delete app;
}

TEST_CASE("rates a timer cannot run share the timer it runs instead", "[timers]")
{
  int argc = 1;
  char arg0[] = "test";
  char* argv[] = {arg0, nullptr};
  QCoreApplication* app
      = QCoreApplication::instance() ? nullptr : new QCoreApplication(argc, argv);
  {
    score::Timers timers;
    QObject a;

    auto* one = timers.acquireTimer(&a, 1.);
    CHECK(one->frequency() == 1.);
    for(double f : {0., -5., std::numeric_limits<double>::infinity(),
                    std::numeric_limits<double>::quiet_NaN()})
    {
      INFO(f);
      CHECK(timers.acquireTimer(&a, f) == one);
    }

    auto* fastest = timers.acquireTimer(&a, 100000.);
    CHECK(timers.acquireTimer(&a, 1e9) == fastest);
    CHECK(fastest != one);

    auto* sixty = timers.acquireTimer(&a, 60.);
    CHECK(timers.acquireTimer(&a, 60.) == sixty);
    CHECK(sixty != one);
    CHECK(sixty != fastest);

    timers.releaseTimer(&a, one);
    timers.releaseTimer(&a, fastest);
    timers.releaseTimer(&a, sixty);
  }
  delete app;
}

