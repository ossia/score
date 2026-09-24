// An asynchronous RecursiveWatch scan can be stopped before its owner dies.
//
// The Library scans the packages tree on a worker thread; its filters call
// into Library plugin objects. When the application was torn down while a
// scan was still running, a filter ran on a destroyed object. After
// cancelAsync() returns, no filter may run again and no commit queued to the
// GUI thread may execute.
#include <score/tools/RecursiveWatch.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QObject>
#include <QTemporaryDir>
#include <QThread>

#include <catch2/catch_test_macros.hpp>

#include <atomic>

TEST_CASE("a cancelled async scan runs no filter and no commit afterwards", "[recursivewatch]")
{
  int argc = 1;
  char arg0[] = "test";
  char* argv[] = {arg0, nullptr};
  QCoreApplication* app = QCoreApplication::instance()
                              ? nullptr
                              : new QCoreApplication(argc, argv);

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  for(int i = 0; i < 2000; i++)
  {
    QFile f(dir.filePath(QStringLiteral("f%1.tst").arg(i)));
    REQUIRE(f.open(QIODevice::WriteOnly));
  }

  std::atomic_int filtered{0};
  std::atomic_int committed{0};
  QObject context;
  {
    score::RecursiveWatch w;
    w.setWatchedFolder(dir.path().toStdString());
    w.registerWatch(
        "tst", score::RecursiveWatch::AsyncCallbacks{
                   .filter = [&](std::string_view) -> std::function<void()> {
      QThread::usleep(500);
      filtered++;
      return [&] { committed++; };
    }});
    w.scanAsync(&context);

    QElapsedTimer t;
    t.start();
    while(filtered < 20 && t.elapsed() < 10000)
      QThread::msleep(1);
    REQUIRE(filtered >= 20);

    w.cancelAsync();
    const int filteredAtCancel = filtered;

    QThread::msleep(300);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    INFO("filtered at cancel " << filteredAtCancel << ", after " << filtered.load());
    CHECK(filtered == filteredAtCancel);
    CHECK(committed == 0);
    CHECK(filteredAtCancel < 2000);
  }
  delete app;
}
