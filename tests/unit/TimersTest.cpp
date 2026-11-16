#include <score/tools/Timers.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>

#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <limits>

namespace
{
QCoreApplication& app()
{
  static int argc = 1;
  static char arg0[] = "test";
  static char* argv[] = {arg0, nullptr};
  if(auto* existing = QCoreApplication::instance())
    return *existing;
  static QCoreApplication a{argc, argv};
  return a;
}

int ticksDuring(score::HighResolutionTimer& timer, int ms)
{
  int ticks = 0;
  auto c = QObject::connect(&timer, &score::HighResolutionTimer::timeout, [&] { ticks++; });
  QEventLoop loop;
  QTimer::singleShot(ms, &loop, &QEventLoop::quit);
  loop.exec();
  QObject::disconnect(c);
  return ticks;
}
}

TEST_CASE("a rate given as a period shares the timer of the same rate in Hz", "[timers]")
{
  app();
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

TEST_CASE("rates a timer cannot run share the timer it runs instead", "[timers]")
{
  app();
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

TEST_CASE("a timer's next deadline keeps its phase and skips missed slots", "[timers]")
{
  using score::nextTimerDeadline;
  constexpr uint64_t T = 16'666'667;
  constexpr uint64_t D = 1'000'000'000;

  CHECK(nextTimerDeadline(D, D, T) == D + T);
  CHECK(nextTimerDeadline(D, D + 5'000'000, T) == D + T);
  CHECK(nextTimerDeadline(D, D + T - 1, T) == D + T);

  CHECK(nextTimerDeadline(D, D + T, T) == D + 2 * T);
  CHECK(nextTimerDeadline(D, D + 3 * T + T / 2, T) == D + 4 * T);
  CHECK(nextTimerDeadline(D, D + 100 * T, T) == D + 101 * T);
  for(uint64_t late : std::initializer_list<uint64_t>{0, 1, T / 3, T, 2 * T + 7, 50 * T + 1})
  {
    const uint64_t next = nextTimerDeadline(D, D + late, T);
    INFO(late);
    CHECK(next > D + late);
    CHECK(next - (D + late) <= T);
    CHECK((next - D) % T == 0);
  }
}

TEST_CASE("a 60 Hz timer ticks 60 times a second, not 1000/16", "[timers][timing]")
{
  app();
  score::HighResolutionTimer timer{60.};
  QElapsedTimer clock;
  clock.start();
  timer.start();
  const int ticks = ticksDuring(timer, 3000);
  timer.stop();
  const double hz = ticks * 1000. / clock.elapsed();
  INFO("ticks " << ticks << " in " << clock.elapsed() << " ms: " << hz << " Hz");
  CHECK(hz > 58.5);
  CHECK(hz < 61.);
}

TEST_CASE("a timer stopped from its own tick ticks no more", "[timers]")
{
  app();
  score::HighResolutionTimer timer{200.};
  int ticks = 0;
  QObject::connect(&timer, &score::HighResolutionTimer::timeout, [&] {
    ticks++;
    timer.stop();
  });
  timer.start();
  ticksDuring(timer, 100);
  CHECK(ticks == 1);
}

TEST_CASE("a timer back from maximum accuracy ticks again", "[timers]")
{
  app();
  score::HighResolutionTimer timer{200.};
  timer.start();
  timer.setMaximumAccuracy(true);
  timer.setMaximumAccuracy(false);
  CHECK(ticksDuring(timer, 200) > 10);
  timer.stop();
}
