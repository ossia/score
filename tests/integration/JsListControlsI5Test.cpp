// N91 and N52, driven through a real --script run of the application.
//
// - The list inputs of Create Collection, Configure Primitive and Scene Graph
//   Filter (Paths, Tags, Names, Material tags) and the property key / value of
//   Scene Graph Filter are controls: Score.setValue stores a whole list, undo
//   and redo restore it. They used to be plain value inlets: setValue warned
//   and stored nothing.
// - A document saved when they were value inlets loads them as controls and
//   keeps their cables and addresses.
// - Score.pushExecutionValue hands one value to an executing inlet for the
//   next tick only; it is how a script feeds Array to texture. The value lands
//   at the next execution tick, which the stepped renderFrames does not wait
//   for: the script leaves the engine time to tick. With a display, the
//   process exits through a crash in an LLVM static destructor unrelated to
//   this test, so that case does not assert the exit status.
// - Score.setCurvePoints edits 2D and 3D automations, undoably, and refuses
//   non-finite values, short points and fewer than two points.

#include <QByteArray>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
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

QString legacyScore()
{
#if defined(LEGACY_LIST_SCORE)
  return QStringLiteral(LEGACY_LIST_SCORE);
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

bool hasDisplay()
{
  if(qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen")
    return false;
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
  return qEnvironmentVariableIsSet("DISPLAY")
         || qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
#else
  return true;
#endif
}

Run runScript(const QString& path, QStringList extra = {}, bool gpu = false)
{
  auto env = QProcessEnvironment::systemEnvironment();
  if(gpu)
  {
    env.insert("SCORE_FORCE_OFFSCREEN_WINDOW", "Window");
  }
  else
  {
    env.remove("DISPLAY");
    env.remove("WAYLAND_DISPLAY");
    env.insert("QT_QPA_PLATFORM", "offscreen");
  }
  env.insert("SCORE_AUDIO_BACKEND", "dummy");
  env.insert("SCORE_DISABLE_AUDIOPLUGINS", "1");
  env.insert("QT_ASSUME_STDERR_HAS_CONSOLE", "1");
  env.insert("QT_FORCE_STDERR_LOGGING", "1");

  QProcess p;
  p.setProcessEnvironment(env);
  p.setProcessChannelMode(QProcess::MergedChannels);
  extra << QStringList{"--no-gui", "--no-restore", "--wait", "0", "--script", path};
  p.start(appBinary(), extra);

  Run r;
  if(!p.waitForStarted(30000) || !p.waitForFinished(180000))
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

const char* prelude = R"JS(
var s = Score.find("Scenario.1"); if (s) Score.remove(s);
var root = Score.rootInterval();
function make(procName) {
  var p = Score.createProcess(root, procName, "");
  if(!p) { console.log("NO-PROCESS " + procName); Qt.exit(0); }
  return p;
}
function port(proc, portName) {
  var p = Score.inlet(proc, portName);
  if(!p) { console.log("NO-PORT " + portName); Qt.exit(0); }
  return p;
}
function show(label, p) {
  console.log("[T] " + label + " " + Score.valueType(p) + " " + JSON.stringify(p.value));
}
function wait(ms) { var t0 = Date.now(); while(Date.now() - t0 < ms) {} }
)JS";

void skipIfMissing(const Run& r)
{
  if(r.output.contains("NO-PROCESS") || r.output.contains("NO-PORT"))
    SKIP("a process this test needs is not available in this build");
}

int count(const QString& text, const QString& what)
{
  return int(text.count(what));
}
}

TEST_CASE("The scene nodes' list inputs are controls a script can set", "[integration][js][n91]")
{
  if(appBinary().isEmpty() || !QFile::exists(appBinary()))
    SKIP("the score application binary was not built");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  SECTION("every list input stores a whole list")
  {
    auto r = runScript(write(
        dir, "lists.js",
        QByteArray(prelude)
            + R"JS(
var cc = make("Create Collection");
var cp = make("Configure Primitive");
var sg = make("Scene Graph Filter");
var ports = [
  ["cc-paths", port(cc, "Paths")], ["cc-tags", port(cc, "Tags")],
  ["cp-paths", port(cp, "Paths")],
  ["sg-paths", port(sg, "Paths")], ["sg-names", port(sg, "Names")],
  ["sg-tags", port(sg, "Material tags")]];
for(var i = 0; i < ports.length; i++) {
  Score.setValue(ports[i][1], ["/a/*", "b"]);
  show(ports[i][0], ports[i][1]);
}
Qt.exit(0);
)JS"));
    CHECK_FALSE(r.crashed);
    CHECK(r.exitCode == 0);
    skipIfMissing(r);
    CHECK_FALSE(r.output.contains("is not a control"));
    for(auto name : {"cc-paths", "cc-tags", "cp-paths", "sg-paths", "sg-names", "sg-tags"})
    {
      INFO(name);
      CHECK(r.output.contains(
          QStringLiteral("[T] %1 Tuple \"[[10000, /a/*], [10001, b]]\"").arg(name)));
    }
  }

  SECTION("a new list replaces the old one; undo and redo restore it")
  {
    auto r = runScript(write(
        dir, "replace.js",
        QByteArray(prelude)
            + R"JS(
var names = port(make("Scene Graph Filter"), "Names");
show("init", names);
Score.setValue(names, ["Car", "Wheel"]); show("two", names);
Score.setValue(names, ["Door"]); show("one", names);
Score.undo(); show("undo", names);
Score.redo(); show("redo", names);
Score.setValue(names, "Hood"); show("string", names);
Score.setValue(names, []); show("empty", names);
Qt.exit(0);
)JS"));
    CHECK_FALSE(r.crashed);
    skipIfMissing(r);
    CHECK(r.output.contains("[T] init Tuple \"[]\""));
    CHECK(r.output.contains("[T] two Tuple \"[[10000, Car], [10001, Wheel]]\""));
    CHECK(r.output.contains("[T] one Tuple \"[[10000, Door]]\""));
    CHECK(r.output.contains("[T] undo Tuple \"[[10000, Car], [10001, Wheel]]\""));
    CHECK(r.output.contains("[T] redo Tuple \"[[10000, Door]]\""));
    CHECK(r.output.contains("[T] string Tuple \"[[10000, Hood]]\""));
    CHECK(r.output.contains("[T] empty Tuple \"[]\""));
  }

  SECTION("the property key and value of Scene Graph Filter are text controls")
  {
    auto r = runScript(write(
        dir, "props.js",
        QByteArray(prelude)
            + R"JS(
var sg = make("Scene Graph Filter");
var k = port(sg, "Property key"), v = port(sg, "Property value");
Score.setValue(k, "roughness"); show("key", k);
Score.setValue(v, "0.5"); show("value", v);
Qt.exit(0);
)JS"));
    CHECK_FALSE(r.crashed);
    skipIfMissing(r);
    CHECK_FALSE(r.output.contains("is not a control"));
    CHECK(r.output.contains("[T] key String \"roughness\""));
    CHECK(r.output.contains("[T] value String \"0.5\""));
  }
}

TEST_CASE("A document saved with list value inlets loads them as controls", "[integration][js][n91]")
{
  if(appBinary().isEmpty() || !QFile::exists(appBinary()))
    SKIP("the score application binary was not built");
  REQUIRE(QFile::exists(legacyScore()));

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString resaved = dir.filePath("resaved.score");

  auto r = runScript(
      write(
          dir, "legacy.js",
          "var OUT = \"" + resaved.toUtf8() + "\";\n"
              + R"JS(
function show(label, p) {
  console.log("[T] " + label + " " + Score.valueType(p) + " " + JSON.stringify(p.value));
}
var cc = Score.find("Create Collection.1");
var cp = Score.find("Configure Primitive.2");
var sg = Score.find("Scene Graph Filter.3");
if(!cc || !cp || !sg) { console.log("NOT-LOADED"); Qt.exit(0); }
var names = Score.inlet(sg, "Names");
show("names", names);
console.log("[T] names-cables " + Score.cables(names));
console.log("[T] cp-address " + Score.inlet(cp, "Paths").address);
Score.setValue(Score.inlet(cc, "Tags"), ["t1"]);
show("tags", Score.inlet(cc, "Tags"));
Score.saveAs(OUT);
console.log("SAVED");
Qt.exit(0);
)JS"),
      {legacyScore()});
  CHECK_FALSE(r.crashed);
  INFO(r.output.toStdString());
  REQUIRE_FALSE(r.output.contains("NOT-LOADED"));
  CHECK_FALSE(r.output.contains("is not a control"));
  CHECK(r.output.contains("[T] names Tuple \"[]\""));
  CHECK(r.output.contains("[T] names-cables 1"));
  CHECK(r.output.contains("audio:/in/0"));
  CHECK(r.output.contains("[T] tags Tuple \"[[10000, t1]]\""));
  REQUIRE(r.output.contains("SAVED"));

  QFile f{resaved};
  REQUIRE(f.open(QIODevice::ReadOnly));
  const QByteArray json = f.readAll();
  CHECK(json.contains("audio:/in/0"));
  CHECK(json.contains("t1"));
  // Process::ValueInlet's key: no list input is saved as one any more.
  CHECK(json.count("769dd38a-bfb3-4dc6-b52a-b6abb7afe2a3") == 0);

  auto reload = runScript(
      write(
          dir, "reload.js",
          R"JS(
var t = Score.inlet(Score.find("Create Collection.1"), "Tags");
console.log("[T] reloaded " + Score.valueType(t) + " " + JSON.stringify(t.value));
Qt.exit(0);
)JS"),
      {resaved});
  CHECK(reload.output.contains("[T] reloaded Tuple \"[[10000, t1]]\""));
}

TEST_CASE("Score.pushExecutionValue feeds an executing inlet for one tick", "[integration][js][n91]")
{
  if(appBinary().isEmpty() || !QFile::exists(appBinary()))
    SKIP("the score application binary was not built");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  auto r = runScript(write(
      dir, "push.js",
      QByteArray(prelude)
          + R"JS(
var js = Score.createProcess(root, "Javascript",
  "import Score 1.0\nScript {\n  ValueInlet { id: in1; objectName: \"in\" }\n"
  + "  tick: function(token, state) { var vs = in1.values;"
  + " for(var i = 0; i < vs.length; i++) console.log(\"[RX] \" + JSON.stringify(vs[i].value)); }\n}\n");
if(!js) { console.log("NO-PROCESS Javascript"); Qt.exit(0); }
var att = make("Array to texture");
var inl = Score.inlet(js, 0);
console.log("[T] stopped " + Score.pushExecutionValue(inl, [1, 2, 3]));
Score.play();
wait(500);
console.log("[T] playing " + Score.pushExecutionValue(inl, [1, 2, 3]));
console.log("[T] array " + Score.pushExecutionValue(port(att, "Input"), [0.5, 0.5]));
console.log("[T] outlet " + Score.pushExecutionValue(Score.outlet(att, 0), [1]));
wait(800);
Score.stop();
console.log("DONE");
Qt.exit(0);
)JS"));
  CHECK_FALSE(r.crashed);
  skipIfMissing(r);
  INFO(r.output.toStdString());
  REQUIRE(r.output.contains("DONE"));
  CHECK(r.output.contains("[T] stopped false"));
  CHECK(r.output.contains("the score is not executing"));
  CHECK(r.output.contains("[T] playing true"));
  CHECK(r.output.contains("[T] array true"));
  CHECK(r.output.contains("[T] outlet false"));
  CHECK(count(r.output, "[RX] [1,2,3]") == 1);
  CHECK(count(r.output, "[RX]") == 1);
}

TEST_CASE("Score.pushExecutionValue fills Array to texture", "[integration][js][gfx][n91]")
{
  if(appBinary().isEmpty() || !QFile::exists(appBinary()))
    SKIP("the score application binary was not built");
  if(!hasDisplay())
    SKIP("needs a display");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  auto grab = [&](bool push) {
    const QString png = dir.filePath(push ? "push.png" : "nopush.png");
    QFile::remove(png);
    auto r = runScript(
        write(
            dir, push ? "att-push.js" : "att-nopush.js",
            "var OUT = \"" + png.toUtf8() + "\"; var PUSH = "
                + (push ? "true" : "false") + ";\n"
                + R"JS(
Score.createDevice("Window", "5a181207-7d40-4ad8-814e-879fcdf8cc31",
  {"Mode": 1, "Outputs": [], "InputWidth": 64, "InputHeight": 64});
var s = Score.find("Scenario.1"); if (s) Score.remove(s);
var att = Score.createProcess(Score.rootInterval(), "Array to texture", "");
if(!att) { console.log("NO-PROCESS"); Qt.exit(0); }
Score.setValue(Score.inlet(att, "Size"), [2, 2]);
Score.setAddress(Score.outlet(att, 0), "Window:/");
var dev = Score.device("Window");
Score.play();
dev.setStepRate(60);
dev.renderFrames(10);
var px = []; for (var i = 0; i < 4; i++) px.push(255, 0, 255, 255);
if(PUSH) console.log("[T] push " + Score.pushExecutionValue(Score.inlet(att, "Input"), px));
var t0 = Date.now(); while(Date.now() - t0 < 300) {}
dev.renderFrames(30);
dev.grabTo(OUT);
Score.stop();
console.log("GRABBED");
Qt.exit(0);
)JS"),
        {}, true);
    INFO(r.output.toStdString());
    skipIfMissing(r);
    REQUIRE(r.output.contains("GRABBED"));
    if(push)
      CHECK(r.output.contains("[T] push true"));
    const QImage img = QImage{png}.convertToFormat(QImage::Format_RGB32);
    REQUIRE_FALSE(img.isNull());
    return img.pixel(img.width() / 2, img.height() / 2);
  };

  const QRgb pushed = grab(true);
  INFO("pushed=(" << qRed(pushed) << "," << qGreen(pushed) << "," << qBlue(pushed) << ")");
  CHECK(qRed(pushed) > 200);
  CHECK(qGreen(pushed) < 50);
  CHECK(qBlue(pushed) > 200);

  const QRgb idle = grab(false);
  INFO("idle=(" << qRed(idle) << "," << qGreen(idle) << "," << qBlue(idle) << ")");
  CHECK_FALSE((qRed(idle) > 200 && qGreen(idle) < 50 && qBlue(idle) > 200));
}

TEST_CASE("Score.setCurvePoints edits 2D and 3D automations", "[integration][js][n52]")
{
  if(appBinary().isEmpty() || !QFile::exists(appBinary()))
    SKIP("the score application binary was not built");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  auto r = runScript(write(
      dir, "splines.js",
      QByteArray(prelude)
          + R"JS(
function spline(label, p) {
  console.log("[T] " + label + " " + JSON.stringify(JSON.parse(Score.savePreset(p)).Preset.Spline));
}
var a3 = make("3D Automation");
spline("3d-init", a3);
Score.setCurvePoints(a3, [[0, 0, 0], [0.5, 2.5, -3.25], [1, 1, 1]]);
spline("3d-set", a3);
Score.setCurvePoints(a3, [[0, 0, 0], [0.5, NaN, 0], [1, 1, 1]]);
spline("3d-nan", a3);
Score.setCurvePoints(a3, [[0, 0, 0], [1, Infinity, 1]]);
spline("3d-inf", a3);
Score.setCurvePoints(a3, [[0, 0], [1, 1]]);
spline("3d-short", a3);
Score.setCurvePoints(a3, [[0, 0, 0]]);
spline("3d-one", a3);
Score.setCurvePoints(a3, [[0, 0, 0], [1, "x", 1]]);
spline("3d-text", a3);
Score.undo();
spline("3d-undo", a3);
Score.redo();
spline("3d-redo", a3);

var a2 = make("2D Automation");
Score.setCurvePoints(a2, [[0, 0], [0.5, -7.5], [4, 1]]);
spline("2d-set", a2);
Score.setCurvePoints(a2, [[0, 0], [1, NaN]]);
spline("2d-nan", a2);
Score.setCurvePoints(a2, [[0], [1]]);
spline("2d-short", a2);
Qt.exit(0);
)JS"));
  CHECK_FALSE(r.crashed);
  skipIfMissing(r);
  INFO(r.output.toStdString());
  const QString set3 = "[[0,0,0],[0.5,2.5,-3.25],[1,1,1]]";
  const QString init3
      = "[[0,0,0],[0.4,0.075,0.17],[0.45,0.24,0.54],[0.5,0.5,0.35],[0.55,0.76,0.8],"
        "[0.7,0.9,0.1],[1,1,1]]";
  CHECK(r.output.contains("[T] 3d-init " + init3));
  CHECK(r.output.contains("[T] 3d-set " + set3));
  CHECK(r.output.contains("[T] 3d-nan " + set3));
  CHECK(r.output.contains("[T] 3d-inf " + set3));
  CHECK(r.output.contains("[T] 3d-short " + set3));
  CHECK(r.output.contains("[T] 3d-one " + set3));
  CHECK(r.output.contains("[T] 3d-text " + set3));
  CHECK(r.output.contains("[T] 3d-undo " + init3));
  CHECK(r.output.contains("[T] 3d-redo " + set3));
  CHECK(r.output.contains("[T] 2d-set [[0,0],[0.5,-7.5],[4,1]]"));
  CHECK(r.output.contains("[T] 2d-nan [[0,0],[0.5,-7.5],[4,1]]"));
  CHECK(r.output.contains("[T] 2d-short [[0,0],[0.5,-7.5],[4,1]]"));
}
