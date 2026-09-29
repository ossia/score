// Score.pushCommand(): an edit a script performs outside the engine, replayed
// by its handler on undo and redo with the exact payload the script gave, of
// any JSON type, a bare number or string included.
//
// A subprocess test because the handlers belong to the script engine of a
// real --script run.

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

QString runScript(const QString& path, bool& crashed)
{
  auto env = QProcessEnvironment::systemEnvironment();
  env.remove("DISPLAY");
  env.remove("WAYLAND_DISPLAY");
  env.insert("QT_QPA_PLATFORM", "offscreen");
  env.insert("SCORE_AUDIO_BACKEND", "dummy");
  env.insert("SCORE_DISABLE_AUDIOPLUGINS", "1");
  env.insert("QT_ASSUME_STDERR_HAS_CONSOLE", "1");
  env.insert("QT_FORCE_STDERR_LOGGING", "1");

  QProcess p;
  p.setProcessEnvironment(env);
  p.setProcessChannelMode(QProcess::MergedChannels);
  p.start(appBinary(), {"--no-gui", "--no-restore", "--wait", "0", "--script", path});
  crashed = true;
  if(!p.waitForStarted(30000) || !p.waitForFinished(120000))
  {
    p.kill();
    p.waitForFinished(5000);
    return {};
  }
  crashed = p.exitStatus() != QProcess::NormalExit;
  return QString::fromUtf8(p.readAll());
}
}

TEST_CASE("script commands replay their payloads on undo and redo", "[integration][js]")
{
  if(appBinary().isEmpty() || !QFile::exists(appBinary()))
    SKIP("the score application binary was not built");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString path = dir.filePath("commands.js");
  {
    QFile f{path};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(R"JS(
var seen = [];
Score.registerCommandHandler("record", function(v) { seen.push(JSON.stringify(v)); });
Score.registerCommandHandler("throws", function(v) { throw new Error("boom"); });

Score.pushCommand("record", 1, 2);
Score.pushCommand("record", "before", "after");
Score.pushCommand("record", {a: [1, {b: "c"}]}, [true, null, 3.5]);
Score.pushCommand("record", undefined, 0);
Score.pushCommand("throws", 1, 2);
Score.pushCommand("unknown", 1, 2);
console.log("REDO " + seen.join(" "));

seen = [];
for(var i = 0; i < 6; i++)
  Score.undo();
console.log("UNDO " + seen.join(" "));

seen = [];
for(var i = 0; i < 6; i++)
  Score.redo();
console.log("AGAIN " + seen.join(" "));

// A handler registered again replaces the previous one
Score.registerCommandHandler("record", function(v) { seen.push("new:" + JSON.stringify(v)); });
seen = [];
for(var i = 0; i < 4; i++)
  Score.undo();
console.log("REPLACED " + seen.join(" "));
Qt.exit(0);
)JS");
  }

  bool crashed{};
  const auto out = runScript(path, crashed);
  INFO(out.toStdString());
  REQUIRE_FALSE(crashed);
  CHECK(out.contains(R"(REDO 2 "after" [true,null,3.5] 0)"));
  // Newest first; an undefined payload reaches the handler as undefined
  CHECK(out.contains(R"(UNDO  {"a":[1,{"b":"c"}]} "before" 1)"));
  CHECK(out.contains(R"(AGAIN 2 "after" [true,null,3.5] 0)"));
  CHECK(out.contains(R"(REPLACED new:undefined new:{"a":[1,{"b":"c"}]})"));
}
