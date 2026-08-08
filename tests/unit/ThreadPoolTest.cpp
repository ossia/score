// Asking the pool for one thread used to start every thread it would ever hand
// out -- hardware_concurrency()/2 of them, each with the process stack size.
// In a browser that exhausts the worker pool and deadlocks the main thread,
// which is what a media layer's waveform computer hit; on a desktop it is
// merely two dozen threads and a hundred megabytes of stacks for one waveform.

#include <score/tools/ThreadPool.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

#include <catch2/catch_test_macros.hpp>

#include <atomic>

TEST_CASE("The thread pool starts only what it hands out", "[threadpool]")
{
  int argc{};
  QCoreApplication app{argc, nullptr};

  auto& pool = score::ThreadPool::instance();

  auto* first = pool.acquireThread();
  REQUIRE(first);
  CHECK(first->isRunning());

  // The one just handed out, and nothing else: this is the whole point. The
  // eager version started hardware_concurrency()/2 of them here.
  CHECK(pool.startedThreadCount() == 1);

  auto* second = pool.acquireThread();
  REQUIRE(second);
  CHECK(second->isRunning());
  CHECK(pool.startedThreadCount() == 2);

  // Distinct, so the pool is still round-robining rather than handing out one.
  CHECK(first != second);

  pool.releaseThread();
  pool.releaseThread();
}

// Releasing the last thread used to stop the pool, which meant joining the
// workers from whoever released -- in practice the UI thread, in the destructor
// of a sound or video layer. It waited there for as long as the work took. A
// browser never gets that far: a worker only finishes starting once the main
// thread returns to the event loop, so the wait outlasted the tab.
TEST_CASE("Releasing a thread does not wait for its work", "[threadpool]")
{
  int argc{};
  QCoreApplication app{argc, nullptr};

  auto& pool = score::ThreadPool::instance();

  auto* thread = pool.acquireThread();
  REQUIRE(thread);

  // Lives on the pool thread, so it is deleted there too.
  auto* worker = new QObject;
  worker->moveToThread(thread);

  std::atomic_bool running{false};
  std::atomic_bool done{false};
  QMetaObject::invokeMethod(
      worker,
      [&] {
    running = true;
    QThread::msleep(1500);
    done = true;
      },
      Qt::QueuedConnection);

  while(!running)
    QThread::msleep(1);

  QElapsedTimer timer;
  timer.start();
  pool.releaseThread();
  const auto elapsed = timer.elapsed();

  // The work is still going: releasing says "I am done with it", not "stop and
  // let me watch". Joining here would have cost the remainder of the sleep.
  CHECK(!done);
  CHECK(elapsed < 200);

  while(!done)
    QThread::msleep(1);

  // Still usable afterwards: the pool is not torn down under the survivors.
  auto* again = pool.acquireThread();
  REQUIRE(again);
  CHECK(again->isRunning());
  pool.releaseThread();

  worker->deleteLater();
}
