#include <score/tools/Timers.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>

#include <catch2/catch_test_macros.hpp>

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

TEST_CASE("a 60 Hz timer ticks 60 times a second, not 1000/16", "[timers]")
{
  int argc = 1;
  char arg0[] = "test";
  char* argv[] = {arg0, nullptr};
  QCoreApplication* app
      = QCoreApplication::instance() ? nullptr : new QCoreApplication(argc, argv);
  {
    score::HighResolutionTimer timer{60.};
    int ticks = 0;
    QElapsedTimer clock;
    QObject::connect(&timer, &score::HighResolutionTimer::timeout, [&] { ticks++; });
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    clock.start();
    timer.start();
    loop.exec();
    timer.stop();
    const double hz = ticks * 1000. / clock.elapsed();
    INFO("ticks " << ticks << " in " << clock.elapsed() << " ms: " << hz << " Hz");
    CHECK(hz > 58.5);
    CHECK(hz < 61.);
  }
  delete app;
}
