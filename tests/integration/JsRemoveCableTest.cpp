// Score.remove() on a cable.
//
// A cable is parented to the document-level cable map rather than to a
// scenario, so EditJsContext::remove matched neither its ProcessModel branch
// nor the generic parent-based one: the call returned having done nothing, and
// said nothing about it. That is the one operation a script needs to reroute an
// existing connection -- take what an outlet already feeds and feed it from
// somewhere else instead -- so the failure showed up as a patch that was built
// correctly and then left wired to both destinations.
//
// A subprocess test because the behaviour under test is what the scripting API
// does to a live document, which is only reachable through a real --script run.

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
  // The script reports through console.log, which is a qDebug, and the child's
  // stderr here is a pipe: where Qt is built against journald it would log to
  // the journal instead and every check below would read an empty string.
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

// Two processes carrying a texture outlet and a texture inlet, so that a cable
// between them is type-compatible. Any transition shader will do; Fade is the
// smallest of the ones the default package ships.
const char* prelude = R"JS(
var SHADER = "<LIBRARY>:/packages/default/Presets/GLSL_shaders/basic/blend/Fade.fs";
var root = Score.rootInterval();
var a = Score.createProcess(root, "ISF Shader", SHADER);
var b = Score.createProcess(root, "ISF Shader", SHADER);
var c = Score.createProcess(root, "ISF Shader", SHADER);
if(!a || !b || !c) { console.log("NO-SHADER"); Qt.exit(0); }
var out = Score.outlet(a, 0);
var inB = Score.inlet(b, "startImage");
var inC = Score.inlet(c, "startImage");
)JS";
}

TEST_CASE("Score.remove() on a cable", "[integration][js][scripting]")
{
  if(appBinary().isEmpty() || !QFile::exists(appBinary()))
    SKIP("the score application binary was not built");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  SECTION("a removed cable is gone from both of its ports")
  {
    auto r = runScript(write(
        dir, "remove.js",
        QByteArray(prelude)
            + R"JS(
Score.createCable(out, inB);
console.log("BEFORE " + Score.cables(out) + " " + Score.cables(inB));
Score.remove(Score.cable(out, 0));
console.log("AFTER " + Score.cables(out) + " " + Score.cables(inB));
Qt.exit(0);
)JS"));
    CHECK_FALSE(r.crashed);
    CHECK(r.exitCode == 0);
    if(r.output.contains("NO-SHADER"))
      SKIP("the default package's shaders are not installed");
    CHECK(r.output.contains("BEFORE 1 1"));
    // Was "AFTER 1 1": the call did nothing at all.
    CHECK(r.output.contains("AFTER 0 0"));
  }

  SECTION("removal takes effect inside a macro, as a reroute needs")
  {
    auto r = runScript(write(
        dir, "macro.js",
        QByteArray(prelude)
            + R"JS(
Score.createCable(out, inB);
Score.startMacro();
Score.remove(Score.cable(out, 0));
console.log("INSIDE " + Score.cables(out));
Score.createCable(out, inC);
Score.endMacro();
console.log("AFTER " + Score.cables(out));
var moved = Score.cables(out) === 1
         && Score.parentProcess(Score.sink(Score.cable(out, 0))) === c;
console.log("REROUTED " + moved);
Qt.exit(0);
)JS"));
    CHECK_FALSE(r.crashed);
    CHECK(r.exitCode == 0);
    if(r.output.contains("NO-SHADER"))
      SKIP("the default package's shaders are not installed");
    CHECK(r.output.contains("INSIDE 0"));
    CHECK(r.output.contains("AFTER 1"));
    CHECK(r.output.contains("REROUTED true"));
  }

  SECTION("removing something that is not a cable leaves the cable alone")
  {
    // A number cannot become a QObject*, and the engine refuses the call rather
    // than passing null: that is a JS exception the script has to catch, not a
    // crash and not a removal. The port is the interesting case -- it has a
    // parent, so it reaches the generic branch -- and must also be left alone.
    auto r = runScript(write(
        dir, "notacable.js",
        QByteArray(prelude)
            + R"JS(
Score.createCable(out, inB);
function tryRemove(what, v) {
  try { Score.remove(v); console.log("RETURNED " + what); }
  catch(e) { console.log("THREW " + what); }
}
tryRemove("null", null);
tryRemove("outlet", out);
tryRemove("number", 42);
console.log("SURVIVED " + Score.cables(out));
Qt.exit(0);
)JS"));
    CHECK_FALSE(r.crashed);
    CHECK(r.exitCode == 0);
    if(r.output.contains("NO-SHADER"))
      SKIP("the default package's shaders are not installed");
    CHECK(r.output.contains("RETURNED null"));
    CHECK(r.output.contains("RETURNED outlet"));
    CHECK(r.output.contains("THREW number"));
    CHECK(r.output.contains("SURVIVED 1"));
  }
}
