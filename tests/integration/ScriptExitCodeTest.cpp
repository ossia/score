// --script failure reporting: the exit codes and diagnostics of c976ba6c98 and
// c0400df56b.
//
// A subprocess test on purpose: what is under test is the process exit code of
// ossia-score, which an in-process fixture cannot observe. The commit exists
// because scripts failed silently at exit 0; four registered shell harnesses
// pass --script so the happy path breaks loudly, but nothing asserted any of
// the failure paths.

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

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
  int exitCode{};
  bool crashed{};
  QString output;
};

Run runScript(const QString& scriptArg, const QString& configHome = {})
{
  auto env = QProcessEnvironment::systemEnvironment();
  env.remove("DISPLAY");
  env.remove("WAYLAND_DISPLAY");
  env.insert("QT_QPA_PLATFORM", "offscreen");
  env.insert("SCORE_AUDIO_BACKEND", "dummy");
  env.insert("SCORE_DISABLE_AUDIOPLUGINS", "1");
  if(!configHome.isEmpty())
    env.insert("XDG_CONFIG_HOME", configHome);

  QProcess p;
  p.setProcessEnvironment(env);
  p.setProcessChannelMode(QProcess::MergedChannels);
  p.start(
      appBinary(),
      {"--no-gui", "--no-restore", "--wait", "0", "--script", scriptArg});

  Run r;
  if(!p.waitForStarted(30000) || !p.waitForFinished(90000))
  {
    p.kill();
    p.waitForFinished(5000);
    r.crashed = true;
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
}

TEST_CASE("--script reports what happened to it", "[integration][js][script]")
{
  if(appBinary().isEmpty() || !QFile::exists(appBinary()))
    SKIP("the score application binary was not built");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  SECTION("a script that finishes cleanly exits 0")
  {
    auto r = runScript(write(dir, "zero.js", "Qt.exit(0);\n"));
    CHECK_FALSE(r.crashed);
    CHECK(r.exitCode == 0);
  }

  SECTION("Qt.exit(N) reaches the caller")
  {
    // c0400df56b: this used to go through QQmlEngine::quit(), i.e. exit 0, so a
    // script could stop the application but never report that it had failed.
    auto r = runScript(write(dir, "seven.js", "Qt.exit(7);\n"));
    CHECK_FALSE(r.crashed);
    CHECK(r.exitCode == 7);
  }

  SECTION("a throwing script exits 3 and says where")
  {
    const auto path = write(dir, "boom.js", "throw new Error(\"boom\");\n");
    auto r = runScript(path);
    CHECK_FALSE(r.crashed);
    CHECK(r.exitCode == 3);
    CHECK(r.output.contains("--script:"));
    CHECK(r.output.contains(path));
    CHECK(r.output.contains("line 1"));
    CHECK(r.output.contains("boom"));
  }

  // Windows has no geteuid, and clearing the Qt permissions there does not make
  // the file unreadable: NTFS access is decided by ACLs, not by a mode bit.
#if !defined(_WIN32)
  SECTION("an existing but unreadable file exits 2")
  {
    if(::geteuid() == 0)
      SKIP("running as root: mode 000 is still readable");

    const auto path = write(dir, "locked.js", "Qt.exit(0);\n");
    REQUIRE(QFile::setPermissions(path, QFile::Permissions{}));

    auto r = runScript(path);
    CHECK_FALSE(r.crashed);
    CHECK(r.exitCode == 2);
    CHECK(r.output.contains("--script: cannot open"));
    CHECK(r.output.contains(path));
  }
#endif

  SECTION("a path that does not exist is evaluated as a program")
  {
    // Pinning the behaviour, not the intent. JS::stringIsScript ends in an
    // unconditional `return true`, so its punctuation scan is dead code and
    // anything that is not an existing file is classified as inline source: a
    // missing path never reaches the "cannot open" branch, it is parsed
    // (/nonexistent/path.js is a regex literal with invalid flags) and reported
    // as a syntax error. Nonzero either way, but exit 3 rather than the exit 2
    // the commit describes. Tighten this to == 2 if that is ever fixed.
    auto r = runScript(QStringLiteral("/definitely/does/not/exist.js"));
    CHECK_FALSE(r.crashed);
    CHECK(r.exitCode == 3);
    CHECK(r.output.contains("<inline>"));
    CHECK(r.output.contains("SyntaxError"));
    CHECK_FALSE(r.output.contains("--script: cannot open"));
  }

  SECTION("an unreadable file names the path it actually tried")
  {
    const QString cfg = dir.filePath("config");
    REQUIRE(QDir{}.mkpath(cfg));

    const auto path = write(
        dir, "read.js",
        "Util.readFile(\"<LIBRARY>:/definitely/absent.js\");\n"
        "Util.readFile(\"/definitely/absent-plain.js\");\n"
        "Qt.exit(0);\n");

    auto r = runScript(path, cfg);
    CHECK_FALSE(r.crashed);
    CHECK(r.exitCode == 0);
    // Without this an unresolvable read is indistinguishable from an empty
    // file: eval("") is a no-op and the run reports success.
    CHECK(r.output.contains("Score.readFile: cannot read"));
    // The arrow form appears exactly when a prefix was resolved to something
    // else, which is the case a plain missing path must not be confused with.
    CHECK(r.output.contains("<LIBRARY>:/definitely/absent.js ->"));
    CHECK(r.output.contains("Score.readFile: cannot read /definitely/absent-plain.js"));
  }
}
