// Tests for Media::PluginScanner, the shared out-of-process plug-in scan
// machinery, driven end-to-end against the score_test_fake_puppet fixture.
//
// Regressions covered (each was a live bug in the per-backend scanners):
//  * Cross-instance pollution: with the old fixed ports, scan replies from
//    other score processes were appended to this instance's database. Now:
//    replies carrying a wrong session token are dropped, two scanners run
//    fully isolated, and each resolves exactly its own scan set.
//  * Legacy string request ids ("Request":"3") parsed as 0 via
//    QJsonValue::toInt(), attributing every reply to slot 0.
//  * A crash fired both errorOccurred and finished -> two invalid entries.
//  * A reply racing the puppet's exit (possibly with a non-zero exit code
//    from the websocket close handshake) -> plug-in recorded both valid
//    and invalid. The grace period resolves the race in favour of the reply.
//  * Hung puppets in the last batch were only reaped while the scan was
//    saturated -> they leaked forever. Timeouts now always fire.
//  * Rescanning while a scan ran corrupted the bookkeeping (TU-static
//    in-flight counters, index-reuse clobbering unrelated slots).

#include <Media/PluginScanner.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <functional>
#include <memory>

namespace
{
QCoreApplication& app()
{
  static int argc = 1;
  static char arg0[] = "plugin_scanner_test";
  static char* argv[] = {arg0, nullptr};
  // Deliberately leaked: a static Q*Application is destroyed from the atexit
  // chain, after main returns and Qt's own static state is gone, which faults
  // in ~QGuiApplication/~QCoreApplication on Windows. Same pattern as
  // tests/unit/InfiniteScrollerTest.cpp.
  static auto* app = new QCoreApplication{argc, argv};
  return *app;
}

//! Pump the event loop until pred() holds or timeout_ms elapses.
bool spinUntil(const std::function<bool()>& pred, int timeout_ms = 10000)
{
  QElapsedTimer timer;
  timer.start();
  while(!pred())
  {
    if(timer.elapsed() > timeout_ms)
      return false;
    app().processEvents(QEventLoop::AllEvents, 20);
  }
  return true;
}

struct ScanResult
{
  std::vector<QString> scanned;
  std::vector<QJsonObject> replies;
  std::vector<QString> failed;
  std::vector<QString> reasons;
  int done{};

  explicit ScanResult(Media::PluginScanner& s)
  {
    QObject::connect(
        &s, &Media::PluginScanner::scanned, &app(),
        [this](const QString& path, const QJsonObject& obj) {
      scanned.push_back(path);
      replies.push_back(obj);
        });
    QObject::connect(
        &s, &Media::PluginScanner::scanFailed, &app(),
        [this](const QString& path, const QString& reason) {
      failed.push_back(path);
      reasons.push_back(reason);
        });
    QObject::connect(
        &s, &Media::PluginScanner::done, &app(), [this] { done++; });
  }
};

QProcessEnvironment behavior(const char* name)
{
  auto env = QProcessEnvironment::systemEnvironment();
  env.insert("FAKE_PUPPET_BEHAVIOR", name);
  return env;
}

std::unique_ptr<Media::PluginScanner> makeScanner(const char* behavior_name)
{
  app(); // ensure the application object exists first
  auto s = std::make_unique<Media::PluginScanner>("test-scanner");
  s->setPuppet(QStringLiteral(SCORE_FAKE_PUPPET));
  s->setEnvironmentProvider([=] { return behavior(behavior_name); });
  s->setProcessTimeout(3000);
  s->setReplyGracePeriod(500);
  return s;
}

QStringList somePaths(int n, const QString& prefix = "/fake/plugin")
{
  QStringList res;
  for(int i = 0; i < n; i++)
    res.push_back(QString("%1-%2.so").arg(prefix).arg(i));
  return res;
}
}

TEST_CASE("scan resolves every path and completes", "[pluginscan][scanner]")
{
  auto s = makeScanner("reply");
  ScanResult r{*s};

  const auto paths = somePaths(5);
  s->setMaxInFlight(2); // force several refill rounds
  s->scan(paths);
  REQUIRE(s->scanning());

  REQUIRE(spinUntil([&] { return r.done > 0; }));

  REQUIRE(r.failed.empty());
  REQUIRE(r.scanned.size() == 5);
  REQUIRE(r.done == 1);
  REQUIRE(!s->scanning());

  // Every requested path resolved exactly once, and the reply payload is
  // the puppet's object
  auto sorted = r.scanned;
  std::sort(sorted.begin(), sorted.end());
  auto expected = std::vector<QString>(paths.begin(), paths.end());
  std::sort(expected.begin(), expected.end());
  REQUIRE(sorted == expected);
  REQUIRE(r.replies[0]["Plugins"].isArray());
}

TEST_CASE("empty scan still completes", "[pluginscan][scanner]")
{
  auto s = makeScanner("reply");
  ScanResult r{*s};
  s->scan({});
  REQUIRE(spinUntil([&] { return r.done > 0; }, 1000));
  REQUIRE(r.scanned.empty());
  REQUIRE(r.failed.empty());
}

TEST_CASE("replies with a wrong session token are dropped", "[pluginscan][scanner][token]")
{
  auto s = makeScanner("badtoken");
  ScanResult r{*s};

  s->scan(somePaths(2));
  REQUIRE(spinUntil([&] { return r.done > 0; }));

  // The impostor replies must not surface as scanned plug-ins; the paths
  // resolve as failed once the grace period elapses.
  REQUIRE(r.scanned.empty());
  REQUIRE(r.failed.size() == 2);
}

TEST_CASE("legacy string request ids are accepted", "[pluginscan][scanner][compat]")
{
  auto s = makeScanner("stringid");
  ScanResult r{*s};

  s->scan(somePaths(3));
  REQUIRE(spinUntil([&] { return r.done > 0; }));

  REQUIRE(r.failed.empty());
  REQUIRE(r.scanned.size() == 3);
}

TEST_CASE("an Error reply fails the plug-in with its reason", "[pluginscan][scanner]")
{
  auto s = makeScanner("error");
  ScanResult r{*s};

  s->scan(somePaths(1));
  REQUIRE(spinUntil([&] { return r.done > 0; }));

  REQUIRE(r.scanned.empty());
  REQUIRE(r.failed.size() == 1);
  REQUIRE(r.reasons[0] == "simulated failure");
}

TEST_CASE("a crashing puppet fails its plug-in exactly once", "[pluginscan][scanner]")
{
  auto s = makeScanner("crash");
  ScanResult r{*s};

  s->scan(somePaths(3));
  REQUIRE(spinUntil([&] { return r.done > 0; }));

  // Historically errorOccurred + finished both recorded an invalid entry
  REQUIRE(r.scanned.empty());
  REQUIRE(r.failed.size() == 3);
}

TEST_CASE("a puppet exiting without a reply fails exactly once", "[pluginscan][scanner]")
{
  auto s = makeScanner("exit1");
  ScanResult r{*s};

  s->scan(somePaths(2));
  REQUIRE(spinUntil([&] { return r.done > 0; }));

  REQUIRE(r.scanned.empty());
  REQUIRE(r.failed.size() == 2);
}

TEST_CASE("garbage replies do not resolve a scan as success", "[pluginscan][scanner]")
{
  auto s = makeScanner("garbage");
  ScanResult r{*s};

  s->scan(somePaths(1));
  REQUIRE(spinUntil([&] { return r.done > 0; }));

  REQUIRE(r.scanned.empty());
  REQUIRE(r.failed.size() == 1);
}

TEST_CASE("hung puppets are reaped by the timeout", "[pluginscan][scanner]")
{
  auto s = makeScanner("hang");
  s->setProcessTimeout(700);
  ScanResult r{*s};

  // More paths than maxInFlight: the *tail* batch used to never time out
  // because the reaper only ran while the scan was saturated
  s->setMaxInFlight(2);
  s->scan(somePaths(3));
  REQUIRE(spinUntil([&] { return r.done > 0; }, 20000));

  REQUIRE(r.scanned.empty());
  REQUIRE(r.failed.size() == 3);
}

TEST_CASE("a reply beating the process exit wins over the exit code", "[pluginscan][scanner]")
{
  auto s = makeScanner("reply_exit1");
  ScanResult r{*s};

  s->scan(somePaths(2));
  REQUIRE(spinUntil([&] { return r.done > 0; }));

  // The old implementations recorded these as invalid (or as both valid
  // and invalid) because exit(1) raced the websocket delivery
  REQUIRE(r.failed.empty());
  REQUIRE(r.scanned.size() == 2);
}

TEST_CASE("a missing puppet binary fails every path, without recursion", "[pluginscan][scanner]")
{
  // FailedToStart is emitted synchronously from QProcess::start(); resolving
  // it inline used to recurse start -> errorOccurred -> refill -> start
  // through the whole queue on one stack.
  auto s = makeScanner("reply");
  s->setPuppet(QStringLiteral("/nonexistent/puppet-binary"));
  ScanResult r{*s};

  s->scan(somePaths(50));
  REQUIRE(spinUntil([&] { return r.done > 0; }));

  REQUIRE(r.scanned.empty());
  REQUIRE(r.failed.size() == 50);
  REQUIRE(r.done == 1);
}

TEST_CASE("two concurrent scanners stay fully isolated", "[pluginscan][scanner][token]")
{
  // The original sin: fixed ports meant one instance received every other
  // instance's replies and appended them to its database.
  auto s1 = makeScanner("reply");
  auto s2 = makeScanner("reply");
  ScanResult r1{*s1};
  ScanResult r2{*s2};

  s1->scan(somePaths(3, "/instance-a/plug"));
  s2->scan(somePaths(4, "/instance-b/plug"));

  REQUIRE(spinUntil([&] { return r1.done > 0 && r2.done > 0; }));

  REQUIRE(s1->port() != s2->port());
  REQUIRE(s1->token() != s2->token());

  REQUIRE(r1.scanned.size() == 3);
  REQUIRE(r2.scanned.size() == 4);
  REQUIRE(r1.failed.empty());
  REQUIRE(r2.failed.empty());
  for(const auto& p : r1.scanned)
    REQUIRE(p.startsWith("/instance-a/"));
  for(const auto& p : r2.scanned)
    REQUIRE(p.startsWith("/instance-b/"));
}

TEST_CASE("rescanning mid-scan cancels cleanly", "[pluginscan][scanner]")
{
  auto s = makeScanner("hang");
  ScanResult r{*s};
  s->setMaxInFlight(2);

  s->scan(somePaths(4, "/old/plug"));
  REQUIRE(s->scanning());

  // Switch behavior and rescan while the first scan's puppets hang.
  // Historically this leaked the in-flight count (a TU-level static),
  // permanently throttling or deadlocking every later scan.
  s->setEnvironmentProvider([] { return behavior("reply"); });
  s->scan(somePaths(3, "/new/plug"));

  REQUIRE(spinUntil([&] { return r.done > 0; }));

  REQUIRE(r.done == 1);
  REQUIRE(r.scanned.size() == 3);
  for(const auto& p : r.scanned)
    REQUIRE(p.startsWith("/new/"));
  // The cancelled scan must not surface as failures either
  REQUIRE(r.failed.empty());
}

TEST_CASE("processIncomingMessage validates before touching anything", "[pluginscan][scanner][token]")
{
  auto s = makeScanner("reply");
  ScanResult r{*s};

  SECTION("valid token, unknown request id")
  {
    s->processIncomingMessage(
        QString(R"({"Token":"%1","Request":123,"Path":"/x"})").arg(s->token()));
    REQUIRE(r.scanned.empty());
    REQUIRE(r.failed.empty());
  }
  SECTION("wrong token")
  {
    s->processIncomingMessage(R"({"Token":"nope","Request":0,"Path":"/x"})");
    REQUIRE(r.scanned.empty());
  }
  SECTION("not JSON")
  {
    s->processIncomingMessage("ceci n'est pas du json");
    REQUIRE(r.scanned.empty());
  }
  SECTION("no token field at all")
  {
    s->processIncomingMessage(R"({"Request":0,"Path":"/x"})");
    REQUIRE(r.scanned.empty());
  }
}
