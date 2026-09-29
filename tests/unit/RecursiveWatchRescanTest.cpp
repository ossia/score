// A new asynchronous scan of a RecursiveWatch (the library's rescan) cancels
// the one still running: none of the old scan's filters run afterwards and
// none of its commits reach the GUI thread, while the new scan delivers all of
// its own. cancelAll() stops the scans of every watch.

#include <score/tools/RecursiveWatch.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QObject>
#include <QTemporaryDir>
#include <QThread>

#include <catch2/catch_test_macros.hpp>

#include <atomic>

namespace
{
constexpr int file_count = 400;

void makeFiles(const QTemporaryDir& dir)
{
  for(int i = 0; i < file_count; i++)
  {
    QFile f(dir.filePath(QStringLiteral("f%1.tst").arg(i)));
    REQUIRE(f.open(QIODevice::WriteOnly));
  }
}

struct Counters
{
  std::atomic_int filtered{0};
  std::atomic_int committed{0};
};

void watch(score::RecursiveWatch& w, const QTemporaryDir& dir, Counters& c, int sleep_us)
{
  w.reset();
  w.setWatchedFolder(dir.path().toStdString());
  w.registerWatch(
      "tst", score::RecursiveWatch::AsyncCallbacks{
                 .filter = [&c, sleep_us](std::string_view) -> std::function<void()> {
    if(sleep_us > 0)
      QThread::usleep(sleep_us);
    c.filtered++;
    return [&c] { c.committed++; };
  }});
}

template <typename F>
bool waitFor(F&& cond, int ms = 10000)
{
  QElapsedTimer t;
  t.start();
  while(!cond() && t.elapsed() < ms)
  {
    QCoreApplication::processEvents();
    QThread::msleep(1);
  }
  return cond();
}

struct App
{
  int argc = 1;
  char arg0[5] = "test";
  char* argv[2] = {arg0, nullptr};
  QCoreApplication* app
      = QCoreApplication::instance() ? nullptr : new QCoreApplication(argc, argv);
  ~App() { delete app; }
};
}

TEST_CASE("a rescan cancels the scan still running", "[recursivewatch]")
{
  App app;
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  makeFiles(dir);

  QObject context;
  score::RecursiveWatch w;
  Counters first, second;

  watch(w, dir, first, 1000);
  w.scanAsync(&context);
  REQUIRE(waitFor([&] { return first.filtered >= 5; }));

  watch(w, dir, second, 0);
  const int firstAtRescan = first.filtered;
  w.scanAsync(&context);

  REQUIRE(waitFor([&] { return second.committed == file_count; }));
  QThread::msleep(50);
  QCoreApplication::processEvents();

  CHECK(first.filtered == firstAtRescan);
  CHECK(first.committed == 0);
  CHECK(second.filtered == file_count);
}

TEST_CASE("cancelAll stops the scans of every watch", "[recursivewatch]")
{
  App app;
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  makeFiles(dir);

  QObject context;
  score::RecursiveWatch a, b;
  Counters ca, cb;
  watch(a, dir, ca, 1000);
  watch(b, dir, cb, 1000);
  a.scanAsync(&context);
  b.scanAsync(&context);
  REQUIRE(waitFor([&] { return ca.filtered >= 2 && cb.filtered >= 2; }));

  score::RecursiveWatch::cancelAll();
  const int fa = ca.filtered, fb = cb.filtered;
  QThread::msleep(50);
  QCoreApplication::processEvents();

  CHECK(ca.filtered == fa);
  CHECK(cb.filtered == fb);
  CHECK(ca.committed == 0);
  CHECK(cb.committed == 0);
}
