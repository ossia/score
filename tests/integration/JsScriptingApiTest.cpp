// The ~110 slots of JS::EditJsContext -- the `Score` object of the scripting
// API -- called the way a script actually reaches them: from a real
// ossia-score process started with --script.
//
// Every one of them takes its arguments from Javascript, so every one of them
// can be handed a null, a number where an object belongs, or a handle to an
// object the script has already removed. A slot that returns an error, throws a
// JS exception or does nothing is fine. A slot that takes the process down is
// not: there is no document left to save and no message a user could act on.
//
// A subprocess test because the behaviour under test IS whether the
// application survives: a crash inside a Catch2 process would take the runner
// with it and report nothing. Each case is bracketed by a TRY/OK marker pair,
// so a single run names the call that died rather than only proving that one
// did.
//
// Six defects were found this way and are registered separately below, each
// [!shouldfail] with the correct expectation. Everything else is in the sweep,
// which is a plain regression guard: it passes today and must keep passing.

#include <QFile>
#include <QProcess>
#include <QString>
#include <QStringList>
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

// The last case whose TRY was printed with no matching OK: on an abort that is
// the call that died.
QString lastUnfinished(const QString& log)
{
  QString pending;
  for(const QString& line : log.split('\n'))
  {
    if(const auto i = line.indexOf("TRY "); i >= 0)
      pending = line.mid(i + 4).trimmed();
    else if(const auto j = line.indexOf("OK "); j >= 0)
      if(line.mid(j + 3).trimmed() == pending)
        pending.clear();
  }
  return pending;
}

void requireBinary()
{
  REQUIRE_FALSE(appBinary().isEmpty());
  REQUIRE(QFile::exists(appBinary()));
}

// A box that is created and then removed, so `dangling` and `danglingProc` are
// Javascript handles to destroyed QObjects; plus one live interval and one live
// process for the calls that need a plausible target.
const char* prelude = R"JS(
function T(n, f) {
  console.log('TRY ' + n);
  try { f(); } catch(e) { console.log('THREW ' + n + ' ' + e); }
  console.log('OK ' + n);
}
var root = Score.rootInterval();
var dangling = Score.createBox(root, 0, 1000000, 300);
var danglingProc = Score.createProcess(dangling, "Automation (float)", "");
Score.remove(dangling);
var proc = Score.createProcess(root, "Automation (float)", "");
var box = Score.createBox(root, 0, 1000000, 100);
)JS";

struct Case
{
  const char* name;
  const char* js;
};

// Everything the API survives today. Order matters: the calls that destroy the
// document come last.
const Case survives[] = {
    {"device.noarg", "Score.device()"},
    {"device.null", "Score.device(null)"},
    {"device.num", "Score.device(42)"},
    {"device.missing", R"JS(Score.device("nope"))JS"},
    {"deviceToJson.noarg", "Score.deviceToJson()"},
    {"deviceToJson.null", "Score.deviceToJson(null)"},
    {"deviceToJson.missing", R"JS(Score.deviceToJson("nope:/x"))JS"},
    {"deviceToOSCQuery.null", "Score.deviceToOSCQuery(null)"},
    {"deviceToOSCQuery.missing", R"JS(Score.deviceToOSCQuery("nope:/x"))JS"},
    {"createDevice.noarg", "Score.createDevice()"},
    {"createDevice.nulls", "Score.createDevice(null, null, null)"},
    {"createDevice.badUuid", R"JS(Score.createDevice("d1", "not-a-uuid", {}))JS"},
    {"createDevice.unknownUuid.obj",
     R"JS(Score.createDevice("d3", "e0e6a8d2-6c9d-4e19-b3f2-c99f2e6d6f4d", {}))JS"},
    {"createOSCDevice.noarg", "Score.createOSCDevice()"},
    {"createOSCDevice.nulls", "Score.createOSCDevice(null, null, null, null)"},
    {"createOSCDevice.badports", R"JS(Score.createOSCDevice("o1", "", -1, 99999999))JS"},
    {"connectOSCQuery.nulls", "Score.connectOSCQueryDevice(null, null)"},
    {"connectOSCQuery.badhost", R"JS(Score.connectOSCQueryDevice("q1", "not a url"))JS"},
    {"removeDevice.noarg", "Score.removeDevice()"},
    {"removeDevice.missing", R"JS(Score.removeDevice("nope"))JS"},
    {"createQMLWS.nulls", "Score.createQMLWebSocketDevice(null, null)"},
    {"createQMLWS.badqml",
     R"JS(Score.createQMLWebSocketDevice("w1", "this is not qml {{{"))JS"},
    {"createQMLSerial.nulls", "Score.createQMLSerialDevice(null, null, null)"},
    {"enumerateDevices.noarg", "Score.enumerateDevices()"},
    {"enumerateDevices.bad", R"JS(Score.enumerateDevices("garbage"))JS"},
    {"listenDevice.missing", R"JS(Score.listenDevice("nope"))JS"},
    {"listenDevice.null", "Score.listenDevice(null)"},
    {"iterateDevice.missing", R"JS(Score.iterateDevice("nope", function(a){}))JS"},
    {"iterateDevice.notfn", R"JS(Score.iterateDevice("nope", 42))JS"},
    {"setDeviceLearn.missing", R"JS(Score.setDeviceLearn("nope", true))JS"},

    {"createAddress.nulls", "Score.createAddress(null, null)"},
    {"createAddress.nodevicepart", R"JS(Score.createAddress("bogus", "float"))JS"},
    {"createProcess.nulls", "Score.createProcess(null, null, null)"},
    {"createProcess.badname", R"JS(Score.createProcess(root, "NoSuchProcessAtAll", ""))JS"},
    {"createProcess.onproc", R"JS(Score.createProcess(proc, "Automation (float)", ""))JS"},
    {"loadPreset.nulls", "Score.loadPreset(null, null)"},
    {"loadPreset.notjson", R"JS(Score.loadPreset(proc, "not json at all"))JS"},
    {"loadPreset.emptyjson", R"JS(Score.loadPreset(proc, "{}"))JS"},
    {"savePreset.null", "Score.savePreset(null)"},
    {"setName.nulls", "Score.setName(null, null)"},
    {"setName.emptyname", R"JS(Score.setName(box, ""))JS"},
    {"createBox.nulls", "Score.createBox(null, null, null, null)"},
    {"createBox.badtime", R"JS(Score.createBox(root, "garbage", "garbage", 0))JS"},
    {"createBox.negdur", "Score.createBox(root, 0, -100000, 0)"},
    {"createBox.onproc", "Score.createBox(proc, 0, 100000, 0)"},
    {"createState.null", "Score.createState(null, 0)"},
    {"createState.notevent", "Score.createState(box, 0)"},
    {"createIntervalAfter.null", "Score.createIntervalAfter(null, 0, 0)"},
    {"createIntervalBetween.null", "Score.createIntervalBetween(null, null)"},
    {"setIntervalDuration.null", "Score.setIntervalDuration(null, 1000)"},
    {"setIntervalDuration.neg", "Score.setIntervalDuration(box, -1000)"},
    {"setIntervalMinDuration.null", "Score.setIntervalMinDuration(null, 1000)"},
    {"setIntervalMaxDuration.null", "Score.setIntervalMaxDuration(null, 1000)"},
    {"setIntervalMaxDuration.lt", "Score.setIntervalMaxDuration(box, 1)"},
    {"setIntervalMaxInfinite.null", "Score.setIntervalMaxInfinite(null, true)"},
    {"setIntervalSpeed.null", "Score.setIntervalSpeed(null, 1)"},
    {"setIntervalSpeed.nan", "Score.setIntervalSpeed(box, NaN)"},
    {"setAutoTrigger.null", "Score.setAutoTrigger(null, true)"},
    {"setAutoTrigger.notsync", "Score.setAutoTrigger(box, true)"},
    {"setProcessLoop.null", "Score.setProcessLoop(null, true)"},
    {"setProcessLoop.notproc", "Score.setProcessLoop(box, true)"},

    {"port.nulls", "Score.port(null, null)"},
    {"port.missing", R"JS(Score.port(proc, "nope"))JS"},
    {"inlet.null", "Score.inlet(null, 0)"},
    {"inlet.neg", "Score.inlet(proc, -1)"},
    {"inlet.oob", "Score.inlet(proc, 9999)"},
    {"inlets.null", "Score.inlets(null)"},
    {"outlet.null", "Score.outlet(null, 0)"},
    {"outlet.neg", "Score.outlet(proc, -5)"},
    {"outlet.oob", "Score.outlet(proc, 9999)"},
    {"outlets.null", "Score.outlets(null)"},
    {"createCable.nulls", "Score.createCable(null, null)"},
    {"createCable.reversed",
     "Score.createCable(Score.inlet(proc,0), Score.outlet(proc,0))"},
    {"createCable.same", "Score.createCable(Score.outlet(proc,0), Score.outlet(proc,0))"},
    {"cable.nulls", "Score.cable(null, null)"},
    {"setAddress.nulls", "Score.setAddress(null, null)"},
    {"setAddress.bogus", R"JS(Score.setAddress(Score.outlet(proc,0), "not an address"))JS"},
    {"setValue.null.d", "Score.setValue(null, 1.5)"},
    {"setValue.null.s", R"JS(Score.setValue(null, "x"))JS"},
    {"setValue.null.b", "Score.setValue(null, true)"},
    {"setValue.null.list", "Score.setValue(null, [1,2,3])"},
    {"setValue.notport", "Score.setValue(box, 1.5)"},
    {"valueType.null", "Score.valueType(null)"},
    {"min.null", "Score.min(null)"},
    {"max.null", "Score.max(null)"},
    {"enumValues.null", "Score.enumValues(null)"},
    {"metadata.null", "Score.metadata(null)"},
    {"startState.null", "Score.startState(null)"},
    {"startEvent.null", "Score.startEvent(null)"},
    {"startSync.null", "Score.startSync(null)"},
    {"endState.null", "Score.endState(null)"},
    {"endEvent.null", "Score.endEvent(null)"},
    {"endSync.null", "Score.endSync(null)"},
    {"startState.notinterval", "Score.startState(proc)"},
    {"endSync.notinterval", "Score.endSync(proc)"},
    {"setCurvePoints.null", "Score.setCurvePoints(null, [[0,0],[1,1]])"},
    {"setCurvePoints.empty", "Score.setCurvePoints(proc, [])"},
    {"setCurvePoints.short", "Score.setCurvePoints(proc, [[0]])"},
    {"setCurvePoints.nan", "Score.setCurvePoints(proc, [[NaN,NaN],[1,1]])"},
    {"setCurvePoints.notcurve", "Score.setCurvePoints(box, [[0,0],[1,1]])"},
    {"setSteps.null", "Score.setSteps(null, [0,1])"},
    {"setSteps.empty", "Score.setSteps(proc, [])"},
    {"messages.null", "Score.messages(null)"},
    {"messages.notstate", "Score.messages(box)"},
    {"setMessages.null", "Score.setMessages(null, [])"},
    {"setMessages.garbage", "Score.setMessages(Score.startState(box), [1, 2, 3])"},
    {"replaceAddress.nulls", "Score.replaceAddress(null, null, null)"},
    {"replaceAddress.nullelem", R"JS(Score.replaceAddress([null], "a", "b"))JS"},
    {"automate.nulls", "Score.automate(null, null)"},
    {"automate.bogusaddr", R"JS(Score.automate(box, "not an address"))JS"},
    {"automate.nullport", "Score.automate(box, null)"},

    {"dangling.path", "Score.path(dangling)"},
    {"dangling.setName", R"JS(Score.setName(dangling, "gone"))JS"},
    {"dangling.startState", "Score.startState(dangling)"},
    {"dangling.inlets", "Score.inlets(danglingProc)"},
    {"dangling.metadata", "Score.metadata(dangling)"},
    {"dangling.remove", "Score.remove(dangling)"},
    {"dangling.createProcess",
     R"JS(Score.createProcess(dangling, "Automation (float)", ""))JS"},
    {"dangling.setDuration", "Score.setIntervalDuration(dangling, 1000)"},

    {"find.null", "Score.find(null)"},
    {"find.empty", R"JS(Score.find(""))JS"},
    {"findByLabel.null", "Score.findByLabel(null)"},
    {"path.null", "Score.path(null)"},
    {"findByPath.garbage", R"JS(Score.findByPath("garbage/1/2/3"))JS"},
    {"findByPath.null", "Score.findByPath(null)"},
    {"documentPlugin.null", "Score.documentPlugin(null)"},
    {"documentPlugin.missing", R"JS(Score.documentPlugin("NoSuchPlugin"))JS"},
    {"document", "Score.document()"},

    {"endMacro.unstarted", "Score.endMacro()"},
    {"startMacro.twice", "Score.startMacro(); Score.startMacro(); Score.endMacro()"},
    {"undo.spam", "for(var i=0;i<50;i++) Score.undo()"},
    {"redo.spam", "for(var i=0;i<50;i++) Score.redo()"},

    {"stop.notplaying", "Score.stop()"},
    {"pause.notplaying", "Score.pause()"},
    {"resume.notplaying", "Score.resume()"},
    {"reinitialize.notplaying", "Score.reinitialize()"},
    {"scrub.nan", "Score.scrub(NaN)"},
    {"scrub.neg", "Score.scrub(-1e12)"},
    {"play.null", "Score.play(null)"},
    {"play.notinterval", "Score.play(proc)"},
    {"transport", "Score.transport()"},
    {"stop.after", "Score.stop()"},

    {"readFile.missing", R"JS(Score.readFile("/nonexistent/nope.txt"))JS"},
    {"readFile.null", "Score.readFile(null)"},
    {"readFile.dir", R"JS(Score.readFile("/tmp"))JS"},
    {"relativize.null", "Score.relativizeFilePath(null)"},
    {"locateFilePath.null", "Score.locateFilePath(null)"},

    {"selectedObject", "Score.selectedObject()"},
    {"selectedObjects", "Score.selectedObjects()"},
    {"select.null", "Score.select(null)"},
    {"select.nulllist", "Score.select([null, null])"},
    {"hasProcessUI.null", "Score.hasProcessUI(null)"},
    {"hasProcessUI.notproc", "Score.hasProcessUI(box)"},
    {"hasProcessUI.proc", "Score.hasProcessUI(proc)"},
    {"showProcessUI.null", "Score.showProcessUI(null, true)"},
    {"showProcessUI.noui", "Score.showProcessUI(proc, true)"},
    {"showProcessUI.hidenoui", "Score.showProcessUI(proc, false)"},

    {"availableProcesses", "Score.availableProcesses()"},
    {"availableProtocols", "Score.availableProtocols()"},
    {"serializeAsJson", "Score.serializeAsJson()"},

    {"remove.null", "Score.remove(null)"},
    {"remove.proc", "Score.remove(proc)"},
    {"remove.root", "Score.remove(root)"},
    {"load.missing", R"JS(Score.load("/nonexistent/nope.score"))JS"},
    {"saveAs.unwritable", R"JS(Score.saveAs("/proc/nope/x.score"))JS"},
};

QString sweepSource()
{
  QString src = QString::fromUtf8(prelude);
  for(const auto& c : survives)
    src += QStringLiteral("T(\"%1\", function() { %2; });\n")
               .arg(QString::fromUtf8(c.name), QString::fromUtf8(c.js));
  src += QStringLiteral("console.log('ALL-DONE');\nQt.exit(0);\n");
  return src;
}

// One call, alone in its own process, so nothing before it can be blamed.
Run runOne(const QTemporaryDir& dir, const QString& name, const QString& js)
{
  const QString src
      = QStringLiteral("try { %1; } catch(e) { console.log('THREW ' + e); }\n"
                       "console.log('DONE');\nQt.exit(0);\n")
            .arg(js);
  return runScript(writeScript(dir, name + ".js", src));
}
}

TEST_CASE(
    "the JS scripting API survives hostile arguments", "[integration][js][scripting]")
{
  requireBinary();
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  const auto r = runScript(writeScript(dir, "sweep.js", sweepSource()));
  INFO("last unfinished call: " << lastUnfinished(r.log).toStdString());
  INFO(r.log.right(20000).toStdString());
  CHECK(r.log.contains(QStringLiteral("ALL-DONE")));
  CHECK_FALSE(r.crashed);
  CHECK(r.exitCode == 0);
}

// EditContext.device.cpp:256 guards the missing-protocol case, but only inside
// the `canConvert<QVariantMap>()` branch. Hand createDevice anything that is
// not an object -- a number, a string -- and neither branch runs, LoadDevice is
// submitted with a protocol key nothing is registered under, and
// DeviceDocumentPlugin::createDeviceFromNode calls makeDevice() on the null
// factory (DeviceDocumentPlugin.cpp:180).
//
// The same call with `{}` returns cleanly, which is what makes this specific:
// it is the argument SHAPE, not the unknown uuid, that skips the guard.
TEST_CASE(
    "Score.createDevice: unknown protocol + non-object settings",
    "[integration][js][scripting]")
{
  requireBinary();
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  const auto r = runOne(
      dir, "createDevice",
      R"JS(Score.createDevice("d", "e0e6a8d2-6c9d-4e19-b3f2-c99f2e6d6f4d", 5))JS");
  INFO(r.log.right(8000).toStdString());
  CHECK(r.log.contains(QStringLiteral("DONE")));
  CHECK(r.exitCode == 0);
}

// Explorer::NodeUpdateProxy::addAddress looks the device up with
// Device::DeviceList::device(), whose only failure mode is SCORE_ASSERT
// (DeviceList.cpp:35) -- a throwing assert nothing catches, so the process
// terminates. Naming a device that does not exist is an ordinary scripting
// mistake, and `Score.createAddress("bogus", ...)` -- no device part at all --
// is already handled without dying.
TEST_CASE(
    "Score.createAddress: address on a device that does not exist",
    "[integration][js][scripting]")
{
  requireBinary();
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  const auto r
      = runOne(dir, "createAddress", R"JS(Score.createAddress("nodevicehere:/x", "float"))JS");
  INFO(r.log.right(8000).toStdString());
  CHECK(r.log.contains(QStringLiteral("DONE")));
  CHECK(r.exitCode == 0);
}

// EditContext.ui.cpp:31 dereferences doc->document.view() before testing it:
// the `if(!main_view)` guard below is on the qobject_cast result, not on the
// view. With --no-gui there is no DocumentView at all, so viewDelegate() is
// called on nullptr. --no-gui --script is the mode this whole API exists for.
TEST_CASE(
    "Score.zoom with no document view", "[integration][js][scripting]")
{
  requireBinary();
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  const auto r = runOne(dir, "zoom", "Score.zoom(1, 1)");
  INFO(r.log.right(8000).toStdString());
  CHECK(r.log.contains(QStringLiteral("DONE")));
  CHECK(r.exitCode == 0);
}

// EditContext.ui.cpp:44, the same shape as zoom().
TEST_CASE(
    "Score.scroll with no document view", "[integration][js][scripting]")
{
  requireBinary();
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  const auto r = runOne(dir, "scroll", "Score.scroll(1, 1)");
  INFO(r.log.right(8000).toStdString());
  CHECK(r.log.contains(QStringLiteral("DONE")));
  CHECK(r.exitCode == 0);
}

// EditContext.introspection.cpp:78 reads the library through
// ApplicationComponents::panel<Library::ProcessPanel>(), which ends in
// SCORE_ABORT when no such panel was instantiated (ApplicationComponents.hpp:145).
// --no-gui instantiates no panels. Score.availableProcesses(), which reads the
// factory list instead of the panel, works in the same process.
TEST_CASE(
    "Score.libraryEntries with no Library panel",
    "[integration][js][scripting]")
{
  requireBinary();
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  const auto r = runOne(dir, "libraryEntries", R"JS(Score.libraryEntries(""))JS");
  INFO(r.log.right(8000).toStdString());
  CHECK(r.log.contains(QStringLiteral("DONE")));
  CHECK(r.exitCode == 0);
}

// EditContext.introspection.cpp:42, the same panel lookup.
TEST_CASE(
    "Score.availableProcessesAndPresets with no Library panel",
    "[integration][js][scripting]")
{
  requireBinary();
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  const auto r = runOne(dir, "presets", "Score.availableProcessesAndPresets()");
  INFO(r.log.right(8000).toStdString());
  CHECK(r.log.contains(QStringLiteral("DONE")));
  CHECK(r.exitCode == 0);
}
