// score::FileWatch must not leave a timer armed on a thread it does not own.
//
// FileWatch polls the watched paths with a QObject timer and runs the actual
// stat() sweep off the GUI thread. It used to do that by calling startTimer()
// and *then* moveToThread()ing itself onto a score::ThreadPool thread, which
// left the 500 ms timer registered in that thread's event dispatcher while
// ~Application destroyed the FileWatch from the GUI thread
// (Application.cpp: `svc.filewatch.reset()`).
//
// Qt cannot unregister a timer from a foreign thread: ~QObjectPrivate warns
// "QObject::~QObject: Timers cannot be stopped from another thread" and leaves
// the registration alone. The pool thread then kept ticking a dangling
// QObject*, and the next tick dispatched a QTimerEvent into the freed block
// (QTimerInfoList::activateTimers -> QCoreApplication::notifyInternal2) --
// an access violation during shutdown, 0xC0000005 on Windows.
//
// Three things are pinned here:
//  1. the FileWatch never changes thread affinity, so its timer is always
//     killed by the thread that armed it;
//  2. destroying one emits no cross-thread timer warning;
//  3. the sweep still runs on a worker thread, not on the caller's -- the
//     whole point of the pool thread in the first place.
#include <score/tools/FileWatch.hpp>
#include <score/tools/ThreadPool.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QObject>
#include <QTemporaryDir>
#include <QThread>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace
{
std::mutex g_msgMutex;
std::vector<QString> g_messages;
QtMessageHandler g_previous{};

void collect(QtMsgType t, const QMessageLogContext& ctx, const QString& msg)
{
  {
    std::lock_guard l{g_msgMutex};
    g_messages.push_back(msg);
  }
  if(g_previous)
    g_previous(t, ctx, msg);
}

bool sawCrossThreadTimerWarning()
{
  std::lock_guard l{g_msgMutex};
  for(const auto& m : g_messages)
    if(m.contains(QStringLiteral("Timers cannot be stopped from another thread")))
      return true;
  return false;
}

//! Pump the calling thread's event loop for @p ms, so queued work and
//! DeferredDelete are delivered the way they are during a real shutdown.
void pumpFor(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
  {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    QThread::msleep(2);
  }
}
}

TEST_CASE("a FileWatch keeps its timer on its own thread", "[filewatch]")
{
  int argc = 1;
  char arg0[] = "test";
  char* argv[] = {arg0, nullptr};
  QCoreApplication* app
      = QCoreApplication::instance() ? nullptr : new QCoreApplication(argc, argv);

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString watched = dir.filePath(QStringLiteral("watched.txt"));
  {
    QFile f{watched};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write("1");
  }

  // Hold a pool thread of our own, so releasing the FileWatch's does not stop
  // the pool: an orphaned timer then still has a live dispatcher to tick on,
  // which is the configuration the shutdown crash happens in.
  auto& pool = score::ThreadPool::instance();
  QThread* held = pool.acquireThread();
  REQUIRE(held != nullptr);

  {
    std::lock_guard l{g_msgMutex};
    g_messages.clear();
  }
  g_previous = qInstallMessageHandler(&collect);

  QThread* const self = QThread::currentThread();
  std::atomic<QThread*> sweepThread{nullptr};
  std::atomic_int sweeps{0};

  {
    score::FileWatch fw;

    // (1) The object must still belong to us: that is what makes killTimer()
    // in ~FileWatch effective. With the old code this is the pool thread.
    CHECK(fw.thread() == self);

    auto cb = std::make_shared<std::function<void()>>([&] {
      sweepThread.store(QThread::currentThread());
      sweeps++;
    });
    fw.add(watched, cb);

    // Touch the file so the next sweep fires the callback. mtime has 1 ms
    // resolution here, and the watch recorded the mtime at add() time.
    QThread::msleep(20);
    {
      QFile f{watched};
      REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Append));
      f.write("2");
    }

    // The timer is 500 ms; give it a few periods.
    QElapsedTimer t;
    t.start();
    while(sweeps == 0 && t.elapsed() < 8000)
    {
      QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
      QThread::msleep(5);
    }

    // (3) The sweep must not have run on the caller's thread.
    INFO("sweeps: " << sweeps.load());
    CHECK(sweeps > 0);
    CHECK(sweepThread.load() != nullptr);
    CHECK(sweepThread.load() != self);

    fw.remove(watched, cb);
  } // ~FileWatch, on this thread -- as ~Application does it.

  // (2) No cross-thread timer teardown.
  CHECK_FALSE(sawCrossThreadTimerWarning());

  // Give an orphaned 500 ms timer every chance to tick into the freed block
  // while the pool is still pumping. With the old code this is where the
  // access violation lands (SIGSEGV / ASan heap-use-after-free in
  // QCoreApplicationPrivate::notify_helper); it is timing-dependent, so it
  // backs up the deterministic checks above rather than replacing them.
  pumpFor(1500);

  qInstallMessageHandler(g_previous);
  g_previous = nullptr;

  pool.releaseThread();
  pumpFor(50);

  delete app;
}
