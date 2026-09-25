#include <QFile>
#include <QProcess>
#include <QString>
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
  QString log;
};

Run runScript(const QString& js)
{
  QProcess p;
  auto env = QProcessEnvironment::systemEnvironment();
  env.insert("ASAN_OPTIONS", "detect_leaks=0:detect_odr_violation=0");
  env.insert("SCORE_AUDIO_BACKEND", "dummy");
  env.insert("SCORE_DISABLE_AUDIOPLUGINS", "1");
  env.insert("QT_FORCE_STDERR_LOGGING", "1");
  env.insert("QT_ASSUME_STDERR_HAS_CONSOLE", "1");
  p.setProcessEnvironment(env);
  p.setProcessChannelMode(QProcess::MergedChannels);
  p.start(appBinary(), {"--no-gui", "--no-restore", "--script", js, "--wait", "0"});

  Run r;
  if(!p.waitForStarted(30000) || !p.waitForFinished(300000))
  {
    p.kill();
    p.waitForFinished(5000);
    r.log = QString::fromUtf8(p.readAll());
    return r;
  }
  r.log = QString::fromUtf8(p.readAll());
  r.crashed = p.exitStatus() != QProcess::NormalExit;
  r.exitCode = p.exitCode();
  return r;
}

QString writeScript(const QTemporaryDir& dir, const QString& name, const QString& src)
{
  const QString path = dir.filePath(name);
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(src.toUtf8());
  f.close();
  return path;
}

const char* script = R"JS(
function fail(what) { console.log("FAIL " + what); Qt.exit(1); }
var root = Score.rootInterval();
var proc = Score.createProcess(root, "bf603921-5a48-4aa5-9bc1-48a762be6467", "");
if(!proc) fail("process");
Score.setName(proc, "fx");
var port = Score.port(proc, "Amount");
if(!port) fail("port");
Score.setScriptable(port, true);
console.log("ADDR " + Score.scriptableAddress(port));

function baseScenario(itv) {
  for(var i = 0; i < Score.processes(itv); i++) {
    var p = Score.process(itv, i);
    if(Score.startState(p)) return p;
  }
  return null;
}
var scenar = baseScenario(root);
if(!scenar) fail("scenario");
var st = Score.startState(scenar);
Score.setMessages(st, [{address: "score:/controls/fx/amount", value: 0.7}]);
console.log("REFS " + Score.references(port).length);
console.log("TARGETS " + Score.targets(st).length);

Score.setName(proc, "reverb");
console.log("MSG " + JSON.stringify(Score.messages(st)));

Score.play();
var t0 = Date.now(); while(Date.now() - t0 < 1500) {}
console.log("VAL " + Device.read("score:/controls/reverb/amount").toFixed(2));
console.log("CTRL " + Score.Controls.reverb.amount.toFixed(2));
Score.stop();

var path = "%1";
Score.saveAs(path);
Score.load(path);
var proc2 = Score.find("reverb");
if(!proc2) fail("reload");
Score.setName(proc2, "delay");
var scenar2 = baseScenario(Score.rootInterval());
if(!scenar2) fail("scenario after reload");
console.log("MSG2 " + JSON.stringify(Score.messages(Score.startState(scenar2))));
console.log("DONE");
Qt.exit(0);
)JS";
}

TEST_CASE(
    "a published control is reached by name from a state, follows a rename, plays, "
    "and survives a reload",
    "[integration][scriptable][script]")
{
  REQUIRE_FALSE(appBinary().isEmpty());
  REQUIRE(QFile::exists(appBinary()));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  const auto src = QString::fromUtf8(script).arg(dir.filePath("doc.score"));
  const auto r = runScript(writeScript(dir, "scriptable.js", src));
  INFO(r.log.right(20000).toStdString());
  CHECK_FALSE(r.crashed);
  CHECK(r.log.contains("DONE"));
  CHECK(r.log.contains("ADDR score:/controls/fx/amount"));
  CHECK(r.log.contains("REFS 1"));
  CHECK(r.log.contains("TARGETS 1"));
  CHECK(r.log.contains("score:/controls/reverb/amount"));
  CHECK(r.log.contains("VAL 0.70"));
  CHECK(r.log.contains("CTRL 0.70"));
  CHECK(r.log.contains("score:/controls/delay/amount"));
}
