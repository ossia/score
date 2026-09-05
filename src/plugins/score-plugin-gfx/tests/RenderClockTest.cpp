// Unit tests for Gfx/Graph/RenderClock.hpp — the render-clock abstraction the
// genlock work introduced (TimerClock / DisplayVSyncClock /
// ExternalGenlockClock).
//
// No card and no GPU: the genlock clock's pull facet is an injected
// std::function precisely so the SDK-free part — the tick thread, the
// marshalling onto the render thread, the phase lock, stop()'s drain — is
// checkable on its own. What a real VBI would add is only *when* the wait
// returns; here a semaphore stands in for the interrupt.
//
// The phase-lock claim is the one that matters: the header promises the next
// VBI wait only begins once the current frame has finished rendering
// (Qt::BlockingQueuedConnection). A clock that queued without blocking would
// let render bursts run ahead of the card; the "waits never outrun ticks"
// bookkeeping below is what catches that.

#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderClock.hpp>

#include <score/tools/Timers.hpp>

#include <QCoreApplication>
#include <QThread>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <semaphore>
#include <thread>

using namespace score::gfx;
using namespace std::chrono;

namespace
{
// Queued connections and BlockingQueuedConnection need a live application
// object and an event dispatcher on this thread.
QCoreApplication& app()
{
  static int argc = 1;
  static char arg0[] = "render-clock-test";
  static char* argv[] = {arg0, nullptr};
  // Deliberately leaked: a static Q*Application is destroyed from the atexit
  // chain, after main returns and Qt's own static state is gone, which faults
  // in ~QGuiApplication/~QCoreApplication on Windows. Same pattern as
  // tests/unit/InfiniteScrollerTest.cpp.
  static auto* a = new QCoreApplication{argc, argv};
  return *a;
}

// Pump the main-thread event queue until pred() or the deadline.
template <typename Pred>
bool pump_until(Pred&& pred, milliseconds deadline = 5000ms)
{
  const auto end = steady_clock::now() + deadline;
  while(!pred())
  {
    if(steady_clock::now() > end)
      return false;
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
  }
  return true;
}
} // namespace

// ---------------------------------------------------------------------------
// TimerClock
// ---------------------------------------------------------------------------

TEST_CASE("a timer clock delivers ticks and stops delivering them")
{
  app();
  score::Timers timers;
  QObject owner;

  TimerClock clock{timers, &owner, 250.};
  CHECK(clock.kind() == RenderClock::Kind::Timer);
  CHECK(clock.frequency() == 250.);

  std::atomic<int> ticks{0};
  clock.start([&] { ++ticks; });

  REQUIRE(pump_until([&] { return ticks.load() >= 3; }));

  clock.stop();
  // Drain anything already queued, then require silence: a stop() that only
  // released the timer but left the connection would keep ticking.
  QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  const int after = ticks.load();
  std::this_thread::sleep_for(30ms);
  QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  CHECK(ticks.load() == after);
}

TEST_CASE("a timer clock's output set is a set")
{
  app();
  score::Timers timers;
  QObject owner;
  TimerClock clock{timers, &owner, 60.};

  // The coalesced set mirrors the old timer->set<OutputNode*> map: adding an
  // output twice must not make it render twice per tick.
  auto* fake1 = reinterpret_cast<OutputNode*>(0x1);
  auto* fake2 = reinterpret_cast<OutputNode*>(0x2);

  CHECK(clock.empty());
  clock.addOutput(fake1);
  clock.addOutput(fake1);
  clock.addOutput(fake2);
  CHECK(clock.outputs().size() == 2);

  clock.addOutput(nullptr); // reload paths hand these around
  CHECK(clock.outputs().size() == 2);

  clock.removeOutput(fake1);
  CHECK(clock.outputs().size() == 1);
  CHECK(clock.outputs()[0] == fake2);
  clock.removeOutput(fake2);
  CHECK(clock.empty());
}

TEST_CASE("timer clocks at one rate share one hardware timer")
{
  app();
  score::Timers timers;
  QObject owner1, owner2;

  // The whole point of the pool: N outputs at the default rate cost one
  // HighResolutionTimer, not N.
  auto* t1 = timers.acquireTimer(&owner1, 120.);
  auto* t2 = timers.acquireTimer(&owner2, 120.);
  CHECK(t1 == t2);
  auto* t3 = timers.acquireTimer(&owner1, 60.);
  CHECK(t3 != t1);
  timers.releaseTimer(&owner1, t1);
  timers.releaseTimer(&owner2, t2);
  timers.releaseTimer(&owner1, t3);
}

// ---------------------------------------------------------------------------
// DisplayVSyncClock
// ---------------------------------------------------------------------------

namespace
{
// The vsync clock's whole contract is that it forwards the tick into the
// output's vsync-callback slot and clears it on stop; the compositor does the
// driving. Only setVSyncCallback matters, the rest is inert.
struct FakeVSyncOutput final : OutputNode
{
  std::function<void()> installed;
  int sets = 0;

  void setVSyncCallback(std::function<void()> cb) override
  {
    ++sets;
    installed = std::move(cb);
  }

  void setRenderer(std::shared_ptr<RenderList>) override { }
  RenderList* renderer() const override { return nullptr; }
  OutputNodeRenderer* createRenderer(RenderList&) const noexcept override
  {
    return nullptr;
  }
  void startRendering() override { }
  void render() override { }
  void stopRendering() override { }
  bool canRender() const override { return false; }
  void onRendererChange() override { }
  void createOutput(OutputConfiguration) override { }
  void destroyOutput() override { }
  std::shared_ptr<RenderState> renderState() const override { return {}; }
  Configuration configuration() const noexcept override { return {}; }
};
} // namespace

TEST_CASE("the vsync clock hands the tick to the output and takes it back")
{
  app();
  FakeVSyncOutput out;

  {
    DisplayVSyncClock clock{out};
    CHECK(clock.kind() == RenderClock::Kind::DisplayVSync);

    int ticks = 0;
    clock.start([&] { ++ticks; });
    REQUIRE(out.installed);

    // The compositor fires its vsync: the render callback runs.
    out.installed();
    out.installed();
    CHECK(ticks == 2);

    clock.stop();
    CHECK_FALSE(out.installed);
  }
  // Destruction after stop() must not clear a callback someone else installed
  // in between — it stops again, which re-clears; that is the documented
  // behaviour, so just require it did not crash and counted its sets.
  CHECK(out.sets >= 2);
}

// ---------------------------------------------------------------------------
// ExternalGenlockClock
// ---------------------------------------------------------------------------

TEST_CASE("a genlock tick fires on the owner's thread, phase-locked")
{
  app();
  QObject owner; // lives on the main thread — standing in for render

  std::counting_semaphore<64> vbi{0};
  std::atomic<int> ticks{0};
  std::atomic<int> grants{0};
  std::atomic<bool> phaseViolation{false};
  std::atomic<bool> wrongThread{false};

  ExternalGenlockClock clock{&owner, [&] {
                               // A granted wait whose tick has not completed
                               // yet means the clock started waiting for the
                               // next VBI before the frame was rendered —
                               // exactly what the blocking invoke forbids.
                               if(ticks.load() != grants.load())
                                 phaseViolation = true;
                               if(!vbi.try_acquire_for(10ms))
                                 return false; // timeout; loop re-checks running
                               ++grants;
                               return true;
                             }};
  CHECK(clock.kind() == RenderClock::Kind::ExternalGenlock);

  clock.start([&] {
    if(QThread::currentThread() != owner.thread())
      wrongThread = true;
    ++ticks;
  });

  vbi.release(3);
  REQUIRE(pump_until([&] { return ticks.load() >= 3; }));

  CHECK_FALSE(wrongThread.load());
  CHECK_FALSE(phaseViolation.load());
  CHECK(grants.load() == 3);

  clock.stop();
}

TEST_CASE("a second start is refused while the clock runs")
{
  app();
  QObject owner;

  std::counting_semaphore<64> vbi{0};
  std::atomic<int> first{0}, second{0};

  ExternalGenlockClock clock{&owner, [&] {
                               if(!vbi.try_acquire_for(10ms))
                                 return false;
                               return true;
                             }};
  clock.start([&] { ++first; });
  clock.start([&] { ++second; }); // must not replace the tick mid-flight

  vbi.release(2);
  REQUIRE(pump_until([&] { return first.load() >= 2; }));
  CHECK(second.load() == 0);

  clock.stop();
}

TEST_CASE("stop() on the owner thread drains a parked tick instead of "
          "dead-locking")
{
  app();
  QObject owner;

  std::counting_semaphore<64> vbi{0};
  std::atomic<int> ticks{0};

  ExternalGenlockClock clock{&owner, [&] {
                               if(!vbi.try_acquire_for(10ms))
                                 return false;
                               return true;
                             }};
  clock.start([&] { ++ticks; });

  // Grant one VBI and give the tick thread time to park itself in the
  // blocking invoke — which cannot complete, because nothing here is
  // processing events.
  vbi.release(1);
  std::this_thread::sleep_for(50ms);
  CHECK(ticks.load() == 0); // parked, not run

  // stop() must pump the owner's queue so the invoke drains, then join.
  // A bare join() here would hang this test into the ctest timeout.
  clock.stop();
  CHECK(ticks.load() == 1); // the in-flight frame was rendered, not dropped

  // And stopping again, or never having started, is inert.
  clock.stop();
  ExternalGenlockClock never{&owner, {}};
  never.stop();
}
