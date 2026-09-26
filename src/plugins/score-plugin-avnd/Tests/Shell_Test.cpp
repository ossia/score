// Shell command: the script runs with the chosen interpreter, or with a custom
// command line where %s stands for the script.
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>

#include <catch2/catch_all.hpp>

#include <examples/Raw/Shell.hpp>

using examples::Shell;

TEST_CASE("the shell command's command line follows the interpreter", "[avnd][shell]")
{
  const std::string script = "echo hello";
  auto line = [&](Shell::Interpreter i, std::string custom = {}) {
    return Shell::commandLine(Shell::job{script, i, std::move(custom)});
  };
  CHECK(line(Shell::System).isEmpty());
  CHECK(line(Shell::Bash) == QStringList{"bash", "-c", "echo hello"});
  CHECK(line(Shell::Zsh) == QStringList{"zsh", "-c", "echo hello"});
  CHECK(line(Shell::Fish) == QStringList{"fish", "-c", "echo hello"});
  CHECK(line(Shell::Python) == QStringList{"python3", "-c", "echo hello"});
  CHECK(line(Shell::Cmd) == QStringList{"cmd", "/C", "echo hello"});
  CHECK(
      line(Shell::Custom, "/opt/foo/bash -c %s")
      == QStringList{"/opt/foo/bash", "-c", "echo hello"});
  CHECK(
      line(Shell::Custom, "\"/opt/my tools/run\" --script=%s")
      == QStringList{"/opt/my tools/run", "--script=echo hello"});
  CHECK(line(Shell::Custom, "/usr/bin/env bash -c") == QStringList{"/usr/bin/env", "bash", "-c", "echo hello"});
  CHECK(line(Shell::Custom, "").isEmpty());
}

#if !defined(_WIN32)
TEST_CASE("the shell command runs its script with the interpreter", "[avnd][shell]")
{
  int argc = 1;
  char arg0[] = "test";
  char* argv[] = {arg0, nullptr};
  QCoreApplication app{argc, argv};

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  for(auto [interp, custom] :
      {std::pair{Shell::Sh, std::string{}}, std::pair{Shell::Custom, std::string{"sh -c %s"}},
       std::pair{Shell::System, std::string{}}})
  {
    const auto out = dir.filePath(QStringLiteral("out-%1").arg(int(interp)));
    Shell::worker::work(
        Shell::job{"printf ran > '" + out.toStdString() + "'", interp, custom});
    QElapsedTimer t;
    t.start();
    while(!QFile::exists(out) && t.elapsed() < 5000)
      QThread::msleep(20);
    QThread::msleep(50);
    QFile f{out};
    REQUIRE(f.open(QIODevice::ReadOnly));
    CHECK(f.readAll() == "ran");
  }
}
#endif
