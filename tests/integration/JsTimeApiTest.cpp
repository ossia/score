// Time in the Javascript scripting API.
//
// Two halves. The first pins the representation that already existed, because
// scripts in the wild depend on it and none of it may shift: a Javascript
// number reaching a TimeVal parameter means *flicks*, Util.toMilliseconds
// converts the other way, and durations are readable off the objects that
// carry them. The second covers what was added -- positions, which were
// reachable from C++ and from nowhere else.
//
// The number/TimeVal conversion in the middle used to depend on magnitude. A
// Javascript number arrives as whichever C++ type it fits in, so an integral
// one below INT32_MAX came through as `int`, which nothing converted to
// TimeVal: every duration under ~3.04 seconds threw, longer ones worked, and
// so did anything fractional. The boundary cases below are that bug.

#include <QByteArray>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace
{
QString appBinary()
{
#if defined(SCORE_APP_BINARY)
  return QStringLiteral(SCORE_APP_BINARY);
#else
  return {};
#endif
}

struct Run
{
  int exitCode{-1};
  bool crashed{true};
  QString output;
};

Run runScript(const QString& path)
{
  auto env = QProcessEnvironment::systemEnvironment();
  env.remove("DISPLAY");
  env.remove("WAYLAND_DISPLAY");
  env.insert("QT_QPA_PLATFORM", "offscreen");
  env.insert("SCORE_AUDIO_BACKEND", "dummy");
  env.insert("SCORE_DISABLE_AUDIOPLUGINS", "1");
  // Every value this test checks is reported through console.log, i.e. a
  // qDebug, and the child's stderr is a pipe: where Qt is built against
  // journald it logs there instead and the checks all read an empty string.
  env.insert("QT_ASSUME_STDERR_HAS_CONSOLE", "1");
  env.insert("QT_FORCE_STDERR_LOGGING", "1");

  QProcess p;
  p.setProcessEnvironment(env);
  p.setProcessChannelMode(QProcess::MergedChannels);
  p.start(
      appBinary(), {"--no-gui", "--no-restore", "--wait", "0", "--script", path});

  Run r;
  if(!p.waitForStarted(30000) || !p.waitForFinished(120000))
  {
    p.kill();
    p.waitForFinished(5000);
    return r;
  }
  r.output = QString::fromUtf8(p.readAll());
  r.crashed = p.exitStatus() != QProcess::NormalExit;
  r.exitCode = p.exitCode();
  return r;
}

QString write(const QTemporaryDir& dir, const QString& name, const QByteArray& body)
{
  const QString path = dir.filePath(name);
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(body);
  f.close();
  return path;
}

// A: 3 s -> 9 s, B: 4 s -> 10 s, so they overlap by 5 s.
const char* prelude = R"JS(
var FL = 705600000;
var root = Score.rootInterval();
var scen = Score.process(root, 0);
var a = Score.createBox(scen, 3*FL, 6*FL, 0.15);
var b = Score.createBox(scen, 4*FL, 6*FL, 0.55);
function say(k, v) { console.log(k + "=" + v); }
function attempt(k, f) { try { f(); say(k, "ok"); } catch(e) { say(k, "threw"); } }
)JS";
}

TEST_CASE("time is readable and writable from Javascript", "[integration][js][time]")
{
  if(appBinary().isEmpty() || !QFile::exists(appBinary()))
    SKIP("the score application binary was not built");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  SECTION("the existing representation is unchanged")
  {
    auto r = runScript(write(
        dir, "contract.js",
        QByteArray(prelude)
            + R"JS(
say("durationMs", Util.toMilliseconds(a.durations.default));
say("minMs", Util.toMilliseconds(a.durations.min));
say("speed", a.durations.speed);
say("rigid", a.durations.isRigid);
say("infinite", Util.isInfinite(a.durations.default));
say("roundtrip", Util.toMilliseconds(Util.timevalFromMilliseconds(4000)));
var p = Score.createProcess(a, "Video", "");
say("startOffsetMs", Util.toMilliseconds(p.startOffset));
say("loopDurationMs", Util.toMilliseconds(p.loopDuration));
Qt.exit(0);
)JS"));
    CHECK_FALSE(r.crashed);
    CHECK(r.exitCode == 0);
    // A number handed to a TimeVal parameter is flicks, and toMilliseconds is
    // the way back. Changing either would silently rescale every script that
    // sets a duration.
    CHECK(r.output.contains("durationMs=6000"));
    CHECK(r.output.contains("minMs=6000"));
    CHECK(r.output.contains("speed=1"));
    CHECK(r.output.contains("rigid=true"));
    CHECK(r.output.contains("infinite=false"));
    CHECK(r.output.contains("roundtrip=4000"));
    CHECK(r.output.contains("startOffsetMs=0"));
    CHECK(r.output.contains("loopDurationMs=6000"));
  }

  SECTION("a number means flicks whatever its magnitude")
  {
    auto r = runScript(write(
        dir, "magnitude.js",
        QByteArray(prelude)
            + R"JS(
attempt("small", function() { Score.setIntervalDuration(a, 1411200000); });
say("smallMs", Util.toMilliseconds(a.durations.default));
attempt("large", function() { Score.setIntervalDuration(a, 10*FL); });
say("largeMs", Util.toMilliseconds(a.durations.default));
attempt("fractional", function() { Score.setIntervalDuration(a, 1000.5); });
say("utilSmall", Util.toMilliseconds(705600));
Qt.exit(0);
)JS"));
    CHECK_FALSE(r.crashed);
    CHECK(r.exitCode == 0);
    // 1411200000 flicks is 2 s and fits in an int: this threw.
    CHECK(r.output.contains("small=ok"));
    CHECK(r.output.contains("smallMs=2000"));
    CHECK(r.output.contains("large=ok"));
    CHECK(r.output.contains("largeMs=10000"));
    CHECK(r.output.contains("fractional=ok"));
    CHECK(r.output.contains("utilSmall=1"));
  }

  SECTION("positions are readable")
  {
    auto r = runScript(write(
        dir, "positions.js",
        QByteArray(prelude)
            + R"JS(
say("aDate", a.date);
say("bDate", b.date);
say("aMarker", a.startMarker);
say("syncDate", Score.startSync(b).date);
say("eventDate", Score.startEvent(b).date);
var p = Score.createProcess(a, "Video", "");
say("procDuration", p.duration);
Qt.exit(0);
)JS"));
    CHECK_FALSE(r.crashed);
    CHECK(r.exitCode == 0);
    CHECK(r.output.contains("aDate=2116800000"));   // 3 s
    CHECK(r.output.contains("bDate=2822400000"));   // 4 s
    CHECK(r.output.contains("aMarker=0"));
    CHECK(r.output.contains("syncDate=2822400000"));
    CHECK(r.output.contains("eventDate=2822400000"));
    CHECK(r.output.contains("procDuration=4233600000")); // 6 s
  }

  SECTION("toSeconds accepts both a position and a duration")
  {
    // The point of the helper: positions come back as raw flicks and durations
    // as a TimeVal, and a script should not have to care which it is holding.
    auto r = runScript(write(
        dir, "seconds.js",
        QByteArray(prelude)
            + R"JS(
say("dateS", Util.toSeconds(a.date));
say("durS", Util.toSeconds(a.durations.default));
var aStart = Util.toSeconds(a.date), aEnd = aStart + Util.toSeconds(a.durations.default);
var bStart = Util.toSeconds(b.date), bEnd = bStart + Util.toSeconds(b.durations.default);
say("earlier", aStart <= bStart ? "A" : "B");
say("overlap", Math.min(aEnd, bEnd) - Math.max(aStart, bStart));
Qt.exit(0);
)JS"));
    CHECK_FALSE(r.crashed);
    CHECK(r.exitCode == 0);
    CHECK(r.output.contains("dateS=3"));
    CHECK(r.output.contains("durS=6"));
    // Which clip is outgoing, and by how much they overlap: the two questions a
    // transition has to answer, and neither was answerable before.
    CHECK(r.output.contains("earlier=A"));
    CHECK(r.output.contains("overlap=5"));
  }
}
