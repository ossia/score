// P1-18 (SPEC-SCENE-RENDER-TESTS.md) -- a gfx process inside a sub-interval
// starts and stops cleanly during playback.
//
// A time-bounded box is created inside the document's OWN nested Scenario
// ("Scenario.1", process 0 of the root interval -- the same object every real
// score plays through), via the console JS API:
//
//     Score.startState(scenario)          EditContext.scenario.cpp:440
//     Score.createIntervalAfter(state, dur_flicks, y)   :385
//     Score.createProcess(box, UUID_ISF, shader)
//
// A FLOATING BOX NEVER EXECUTES -- measured, and the reason this test does
// NOT use Score.createBox.
//   `Macro::createBox` (CommandAPI.cpp:53) mints a BRAND NEW start TimeSync at
//   the requested date via CreateTimeSync_Event_State. Only the scenario's own
//   initial sync is ever marked a start point (`ScenarioModel.cpp:64`
//   setStartPoint(true); nothing else in src/ calls it), and
//   `ossia::scenario::get_roots()` (libossia
//   editor/scenario/detail/scenario_execution.cpp:25-39) collects ONLY syncs
//   with `is_start()`. A box whose start sync is not a start point is therefore
//   unreachable and is never executed at all.
//   Measured three ways on this tree: (a) a JS process placed in a
//   Score.createBox box ticks 0 times while the same process on the root
//   interval ticks 249 times in the same run; (b) the same document saved and
//   re-loaded with --autoplay still publishes no GFX-EDGES; (c) editing only
//   `StartState` in that saved document so the box hangs off the scenario's
//   start state (id 0) makes it publish "GFX-EDGES publish 1" and render.
//   All 15 corpus documents that hold a gfx process inside a sub-interval
//   chain that interval from the scenario start, which is what this test now
//   reproduces. (The same floating-box construction is what
//   DropLayerInScenario.cpp:59, DropPresetInScenario.cpp:60,
//   DropPortInScenario.cpp:51 and ScenarioEditor.cpp:120 produce, so a file
//   dropped into empty scenario space is silently non-playing; that is a
//   product question, recorded, not something this test asserts.)
//
// The box runs from t1 = 2 s to t2 = 5 s and holds one ISF process,
// tests/gfx/corpus/isf-control-color.fs, whose default output is EXACTLY
// magenta (255, 0, 255, 255) on the offscreen RGBA8 target (the shader's own
// header documents the round(255*col) mapping). The window device renders
// offscreen (SCORE_FORCE_OFFSCREEN_WINDOW), and the script grabs at three
// wall-clock times: 1.0 s (before t1), 3.5 s (inside t1..t2), 6.5 s (after
// t2).
//
// The three-phase oracle:
//   before t1  the sink's baseline. With no producer connected, the engine's
//              own baseline for an offscreen window is "renders nothing":
//              BackgroundNode::render clears the readback when the render
//              list holds nothing but the output itself, and grabTo then
//              refuses to write a file and says so --
//              "grabTo: nothing rendered into" (WindowDevice.cpp:130-139).
//              So: no PNG, and that exact warning in the log.
//   t1..t2     the process's colour: the PNG exists and every pixel is
//              magenta within tolerance 2 per channel.
//   after t2   EXACTLY back to the baseline: the readback was re-cleared, no
//              PNG, the warning again. This is a real regression detector: a
//              sink that keeps compositing the stopped process would happily
//              write a magenta after.png.
//
// Why the baseline is "renders nothing" and not a second colour: a
// two-producer variant (a root-level baseline shader plus the box's shader,
// both addressed to Window:/) would need the composite order of two
// simultaneous inputs into one sink to be pinned, and no test in the tree
// pins it. The empty baseline is the engine's documented behaviour and makes
// "returns exactly to the baseline" an assertion about the graph edge really
// being gone, not about blending.
//
// The incremental-edit half, following tests/gfx/GfxEdgeConsumeLatch.cpp:
// SCORE_GFX_TRACE=1 makes GfxContext::updateGraph print
//     GFX-EDGES consume old=<n> new=<n> full=<0|1>
// at the exact decision point (GfxContext.cpp:916-919, inside the
// edges_changed.exchange(false) block at GfxContext.cpp:906). full=1 routes
// through recompute_connections() (GfxContext.cpp:921-925), full=0 through
// incrementalEdgeUpdate (GfxContext.cpp:930). The child's merged log is
// split at MARK-BEFORE / MARK-DURING / MARK-AFTER lines the script prints
// right after each grab; both stderr streams interleave in wall-clock order,
// so the t1 consume lands in the BEFORE..DURING segment and the t2 consume in
// the DURING..AFTER segment. Each transition segment must contain at least
// one consume line, every one of them full=0, and the edge-set sizes must
// grow at t1 (new > old) and shrink at t2 (new < old).
//
// Determinism: renderFrames()/grabFrame() step the RENDER side on demand
// (GfxContext.cpp:956; FrameDeterminismTest.cpp proves the step clock), so
// every grab reads back a frame that was synchronously rendered by the grab
// itself -- no vsync race. The TIMELINE side (interval start/stop) is driven
// by the execution engine on the dummy audio backend's own thread, so the
// transitions happen in wall time; the grabs sit >= 1 s away from t1 and t2
// on either side, and the colours are constants, so timeline jitter cannot
// change any pixel value, only which phase a grab lands in.
//
// WHY THE PHASES ARE INJECTED OVER OSC and not busy-waited in one --script
// (measured on the first run of this test, original in-script design): a JS
// busy-wait (`while (Date.now() - t0 < ms) {}`) blocks the application's
// main-thread event loop for the whole playback, and the execution engine's
// setup work for a nested interval that STARTS MID-PLAY -- creating the
// process's execution component and its gfx node/edge -- is served through
// main-thread queues. With the loop blocked, the box started on the audio
// thread's clock but its gfx edge never materialised: every grab (which DOES
// pump the render side synchronously) reported "no process is connected to
// this device's input", during included. Root-level processes don't hit this
// (their edges exist before play), which is why JsGraphE2ETest.cpp gets away
// with in-script busy-waits. So this test drives the phases exactly the way
// tests/integration/live-edit-sweep.sh drives its tick() storms: the setup
// script defines phase functions and leaves the event loop FREE; the C++
// parent injects `phaseBefore()` / `phaseDuring()` / `phaseAfter()` /
// `finish()` over OSC (/script s "..." on udp/6666, the LocalTree script
// node, JS/ApplicationPlugin.cpp:192-201 -- evaluated in the same console
// engine as --script, so the script's globals are in scope), taking the same
// /tmp/score-harness.lock the sweep holds, since port 6666 is global.
//
// NEGATIVE CONTROL (product-side, per the spec: "force the full-rebuild flag
// at t1 -> the trace assertion goes red while the colour assertions stay
// green"): insert
//     m_fullRebuildThisFrame = true;
// in src/plugins/score-plugin-gfx/Gfx/GfxContext.cpp immediately before the
// SCORE_GFX_TRACE fprintf at GfxContext.cpp:916 (inside the
// `if(edges_changed.exchange(false))` block, GfxContext.cpp:906). Both
// transition consumes then print full=1 and take recompute_connections();
// a full rebuild still renders the correct picture, so the three colour
// phases stay green and only the trace assertions go red.
//
// Intended registration in tests/integration/CMakeLists.txt -- same shape as
// test_integration_frame_determinism (the ctest variant of the
// JsGraphE2ETest.cpp block; this test SKIPs itself cleanly when the binary,
// the corpus or a display is missing, so it can be a real ctest):
//
//   # P1-18: a gfx process inside a sub-interval of the document's own
//   # Scenario.1 starts and stops cleanly during playback. Drives the
//   # application binary; needs a real display (the offscreen QPA has no GL
//   # and reads back a flat colour) and SKIPs itself otherwise.
//   if(TARGET score AND TARGET score_plugin_gfx AND NOT EMSCRIPTEN)
//     score_add_test(test_integration_gfx_nested_interval
//       SOURCES GfxNestedIntervalTest.cpp
//       LIBS ${QT_PREFIX}::Gui)
//     target_compile_definitions(test_integration_gfx_nested_interval PRIVATE
//       "SCORE_APP_BINARY=\"$<TARGET_FILE:score>\""
//       "GFX_TEST_CORPUS_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/../gfx/corpus\"")
//     set_tests_properties(test_integration_gfx_nested_interval PROPERTIES
//       TIMEOUT 600 RUN_SERIAL TRUE LABELS "gui")
//   endif()
//
// Recipe notes inherited from JsGraphE2ETest.cpp: device addresses must be
// "Window:/"; a Scenario CREATED from script never executes, which is why
// this uses the document's pre-existing Scenario.1 instead; `var` only in
// the script (QML scopes const/let inside eval).

#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QThread>
#include <QUdpSocket>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdlib>
#include <regex>
#include <string>
#include <vector>

#if defined(Q_OS_UNIX)
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

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

QString corpusDir()
{
#if defined(GFX_TEST_CORPUS_DIR)
  return QStringLiteral(GFX_TEST_CORPUS_DIR);
#else
  return {};
#endif
}

const char* kUuidIsf = "74ca45ff-92c9-44a0-8f1a-754dea05ee1b";
const char* kUuidWindow = "5a181207-7d40-4ad8-814e-879fcdf8cc31";

// isf-control-color.fs default: [1, 0, 1, 1] -> (255, 0, 255) on the
// offscreen non-sRGB RGBA8 target, per the shader's own header.
constexpr int kExpectedR = 255, kExpectedG = 0, kExpectedB = 255;
constexpr int kTolerance = 2;

struct Run
{
  int exitCode{-1};
  bool crashed{true};
  bool sawReady{false};
  QString log;
};

//! One OSC message `/script s <code>` to the app's LocalTree device on
//! udp/6666 -- byte-identical to what `oscsend 127.0.0.1 6666 /script s ...`
//! sends in live-edit-sweep.sh.
void sendScript(QUdpSocket& sock, const QByteArray& code)
{
  auto pad4 = [](QByteArray b) {
    b.append('\0');
    while(b.size() % 4)
      b.append('\0');
    return b;
  };
  QByteArray dgram = pad4("/script") + pad4(",s") + pad4(code);
  sock.writeDatagram(dgram, QHostAddress::LocalHost, 6666);
}

//! Runs the app on the given setup script, then injects the phase functions
//! over OSC at the given offsets (ms, measured from the NESTED-READY line the
//! script prints right after Score.play()). The last injected call must make
//! the app exit.
Run runPhased(
    const QString& js, const std::vector<std::pair<int, QByteArray>>& phases)
{
  auto env = QProcessEnvironment::systemEnvironment();
  // Offscreen render target: with a mapped window the grab reads the SCREEN
  // at its geometry, i.e. the desktop, which is never blank.
  env.insert("SCORE_FORCE_OFFSCREEN_WINDOW", "Window");
  env.insert("SCORE_AUDIO_BACKEND", "dummy");
  env.insert("SCORE_DISABLE_AUDIOPLUGINS", "1");
  // The whole point: GfxContext prints its edge-consume decisions.
  env.insert("SCORE_GFX_TRACE", "1");
  // The platform's own backend, not OpenGL everywhere -- see JsGraphE2ETest.
#if defined(_WIN32)
  constexpr auto defaultApi = "d3d11";
#elif defined(__APPLE__)
  constexpr auto defaultApi = "metal";
#else
  constexpr auto defaultApi = "opengl";
#endif
  env.insert("QSG_RHI_BACKEND", qEnvironmentVariable("SCORE_TEST_API", defaultApi));
  env.remove("QT_QPA_PLATFORM");
  // Both the MARK lines (console.log through Qt's message handler) and the
  // GFX-EDGES lines (raw fprintf(stderr, ...)) must reach the same pipe, in
  // wall-clock order, on every platform.
  env.insert("QT_FORCE_STDERR_LOGGING", "1");
  env.insert("QT_ASSUME_STDERR_HAS_CONSOLE", "1");

  Run r;

#if defined(Q_OS_UNIX)
  // OSC port 6666 is machine-global: serialize against live-edit-sweep.sh by
  // taking the very same advisory lock it holds around each scenario.
  const int lockFd = ::open("/tmp/score-harness.lock", O_CREAT | O_RDWR, 0666);
  if(lockFd >= 0 && ::flock(lockFd, LOCK_EX) != 0)
  {
    // Lock failure is not fatal; the run just risks stray 6666 traffic.
  }
#endif

  QProcess p;
  p.setProcessEnvironment(env);
  p.setProcessChannelMode(QProcess::MergedChannels);
  p.start(appBinary(), {"--no-gui", "--no-restore", "--script", js});

  auto pump = [&](int ms) {
    QElapsedTimer t;
    t.start();
    do
    {
      p.waitForReadyRead(50);
      r.log += QString::fromUtf8(p.readAll());
    } while(t.elapsed() < ms && p.state() == QProcess::Running);
  };

  if(p.waitForStarted(30000))
  {
    // Wait for the setup script to report play started, up to 60 s.
    QElapsedTimer boot;
    boot.start();
    while(boot.elapsed() < 60000 && p.state() == QProcess::Running
          && !r.log.contains("NESTED-READY"))
      pump(100);
    r.sawReady = r.log.contains("NESTED-READY");

    if(r.sawReady)
    {
      QUdpSocket sock;
      QElapsedTimer t0;
      t0.start();
      for(const auto& [at_ms, code] : phases)
      {
        while(t0.elapsed() < at_ms && p.state() == QProcess::Running)
          pump(50);
        sendScript(sock, code);
      }
    }

    if(!p.waitForFinished(60000))
    {
      p.kill();
      p.waitForFinished(5000);
    }
  }
  r.log += QString::fromUtf8(p.readAll());
  r.crashed = p.exitStatus() != QProcess::NormalExit || p.state() != QProcess::NotRunning;
  r.exitCode = p.exitCode();

#if defined(Q_OS_UNIX)
  if(lockFd >= 0)
    ::close(lockFd); // releases the flock
#endif
  return r;
}

QString writeScript(const QTemporaryDir& dir, const QString& name, const QString& src)
{
  const QString path = dir.filePath(name);
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(src.toUtf8());
  return path;
}

struct ColourVerdict
{
  bool loaded{false};
  int checked{0};
  int offPixels{0};  //!< pixels with any channel further than kTolerance
  int worst{0};      //!< worst per-channel deviation seen
};

//! Every pixel must be the expected colour within kTolerance per channel.
ColourVerdict readUniform(const QString& path)
{
  ColourVerdict v;
  QImage img{path};
  if(img.isNull())
    return v;
  v.loaded = true;
  img = img.convertToFormat(QImage::Format_RGB32);
  for(int y = 0; y < img.height(); y++)
  {
    for(int x = 0; x < img.width(); x++)
    {
      const QRgb px = img.pixel(x, y);
      const int d = std::max(
          {std::abs(qRed(px) - kExpectedR), std::abs(qGreen(px) - kExpectedG),
           std::abs(qBlue(px) - kExpectedB)});
      v.checked++;
      v.worst = std::max(v.worst, d);
      if(d > kTolerance)
        v.offPixels++;
    }
  }
  return v;
}

struct Consume
{
  long oldN{};
  long newN{};
  int full{};
};

//! Every "GFX-EDGES consume old=<n> new=<n> full=<d>" line in a log segment,
//! in order -- the exact line GfxContext.cpp:916-919 emits, parsed the way
//! GfxEdgeConsumeLatch.cpp parses it.
std::vector<Consume> consumes(const QString& segment)
{
  static const std::regex re{"GFX-EDGES consume old=(\\d+) new=(\\d+) full=(\\d)"};
  const std::string s = segment.toStdString();
  std::vector<Consume> out;
  for(auto it = std::sregex_iterator(s.begin(), s.end(), re);
      it != std::sregex_iterator(); ++it)
    out.push_back(
        {std::stol((*it)[1].str()), std::stol((*it)[2].str()),
         (*it)[3].str() == "1" ? 1 : 0});
  return out;
}

bool ready()
{
  if(appBinary().isEmpty() || !QFile::exists(appBinary()))
    return false;
  if(corpusDir().isEmpty() || !QFile::exists(corpusDir() + "/isf-control-color.fs"))
    return false;
  // The offscreen QPA has no GL: the readback comes back flat, proving
  // nothing. Same convention as FrameDeterminismTest.cpp.
  if(qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen")
    return false;
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
  return qEnvironmentVariableIsSet("DISPLAY")
         || qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
#else
  return true;
#endif
}
}

TEST_CASE(
    "a gfx process inside a sub-interval starts and stops cleanly during "
    "playback",
    "[integration][gfx][js][scenario]")
{
  if(!ready())
    SKIP("needs the score binary, the gfx corpus and a display");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  if(qEnvironmentVariableIsSet("SCORE_TEST_KEEP_ARTIFACTS"))
  {
    dir.setAutoRemove(false);
    WARN("artifacts kept in " << dir.path().toStdString());
  }

  const QString before = dir.filePath("before.png");
  const QString during = dir.filePath("during.png");
  const QString after = dir.filePath("after.png");

  // t1 = 2 s, t2 = 5 s; grabs at 1.0 / 3.5 / 6.5 s, >= 1 s from any edge.
  QString src;
  src += "var FL = 705600000;\n"; // flicks per second (TimeVal impl units)
  src += QStringLiteral("var UUID_ISF = \"%1\";\n").arg(kUuidIsf);
  src += QStringLiteral("var UUID_WINDOW = \"%1\";\n").arg(kUuidWindow);
  src += "Score.createDevice(\"Window\", UUID_WINDOW, {});\n";
  src += "var root = Score.rootInterval();\n";
  src += "Score.setIntervalDuration(root, 10 * FL);\n";
  // The document's own nested Scenario -- NOT one created from script, which
  // would never execute (JsGraphE2ETest.cpp recipe note).
  src += "var scen = Score.process(root, 0);\n";
  src += "if (!scen) { console.log(\"NESTED-ERROR: no default scenario\"); "
         "Qt.exit(9); }\n";
  // The box is CHAINED from the scenario's own start point, not dropped as a
  // free-floating box: an empty 0 -> 2 s lead interval, then the 2 -> 5 s box
  // that holds the process. This is the topology every real score has (all 15
  // corpus documents with a gfx process inside a sub-interval look like this),
  // and it is REQUIRED, not cosmetic -- see the "floating box" note in the
  // header.
  src += "var s0 = Score.startState(scen);\n";
  src += "if (!s0) { console.log(\"NESTED-ERROR: no start state\"); "
         "Qt.exit(13); }\n";
  src += "var lead = Score.createIntervalAfter(s0, 2 * FL, 0.3);\n";
  src += "if (!lead) { console.log(\"NESTED-ERROR: no lead\"); Qt.exit(14); }\n";
  src += "var box = Score.createIntervalAfter(Score.endState(lead), 3 * FL, 0.3);\n";
  src += "if (!box) { console.log(\"NESTED-ERROR: no box\"); Qt.exit(10); }\n";
  src += "var proc = Score.createProcess(box, UUID_ISF, \"" + corpusDir()
         + "/isf-control-color.fs\");\n";
  src += "if (!proc) { console.log(\"NESTED-ERROR: no process\"); Qt.exit(11); }\n";
  src += "Score.setAddress(Score.outlet(proc, 0), \"Window:/\");\n";
  src += "var dev = Score.device(\"Window\");\n";
  src += "if (!dev) { console.log(\"NESTED-ERROR: no window device\"); "
         "Qt.exit(12); }\n";
  // The phase functions the parent injects over OSC. Defined here so the
  // event loop stays free between phases (see the header: a mid-play nested
  // start needs the main-thread queues running).
  src += "function phaseBefore() { dev.grabFrame(2, \"" + before
         + "\"); console.log(\"MARK-BEFORE\"); }\n";
  src += "function phaseDuring() { dev.grabFrame(2, \"" + during
         + "\"); console.log(\"MARK-DURING\"); }\n";
  src += "function phaseAfter() { dev.grabFrame(2, \"" + after
         + "\"); console.log(\"MARK-AFTER\"); }\n";
  src += "function finish() { Score.stop(); console.log(\"NESTED-OK\"); "
         "Qt.exit(0); }\n";
  src += "Score.play();\n";
  src += "console.log(\"NESTED-READY\");\n";

  const auto r = runPhased(
      writeScript(dir, "nested.js", src),
      {{1000, "phaseBefore()"},
       {3500, "phaseDuring()"},
       {6500, "phaseAfter()"},
       {7000, "finish()"}});
  INFO(r.log.toStdString());
  REQUIRE(r.sawReady);
  CHECK_FALSE(r.crashed);
  CHECK(r.exitCode == 0);
  REQUIRE(r.log.contains("NESTED-OK"));
  // The offscreen forcing worked; nothing grabbed the desktop.
  REQUIRE_FALSE(r.log.contains("capturing the SCREEN"));

  // Split the merged log into phase segments at the MARK lines.
  const auto iB = r.log.indexOf("MARK-BEFORE");
  const auto iD = r.log.indexOf("MARK-DURING");
  const auto iA = r.log.indexOf("MARK-AFTER");
  REQUIRE(iB >= 0);
  REQUIRE(iD > iB);
  REQUIRE(iA > iD);
  const QString segBefore = r.log.left(iB);           // start .. 1.0 s grab
  const QString segToDuring = r.log.mid(iB, iD - iB); // crosses t1 = 2 s
  const QString segToAfter = r.log.mid(iD, iA - iD);  // crosses t2 = 5 s

  // -- Phase 1: before t1 the sink is at its baseline: nothing is connected,
  // so the grab writes no file and the engine says exactly why.
  CHECK_FALSE(QFile::exists(before));
  CHECK(segBefore.contains("nothing rendered into"));

  // -- Phase 2: between t1 and t2 the output is the process's colour, every
  // pixel, within tolerance 2.
  {
    REQUIRE(QFile::exists(during));
    const auto v = readUniform(during);
    INFO(
        "during.png: checked " << v.checked << " pixels, " << v.offPixels
                               << " off-colour, worst deviation " << v.worst);
    REQUIRE(v.loaded);
    REQUIRE(v.checked > 0);
    REQUIRE(v.offPixels == 0);
    CHECK_FALSE(segToDuring.contains("nothing rendered into"));
  }

  // -- Phase 3: after t2 the sink is EXACTLY back at the baseline: the
  // readback was re-cleared (BackgroundNode::render), no file was written. A
  // stale magenta after.png here is the "did not stop cleanly" failure.
  CHECK_FALSE(QFile::exists(after));
  CHECK(segToAfter.contains("nothing rendered into"));

  // -- The graph was edited INCREMENTALLY at both transitions: every
  // GFX-EDGES consume in each transition segment says full=0, and the edge
  // set grew at t1 and shrank at t2. A full rebuild (full=1) is the
  // regression this half exists for.
  {
    const auto atT1 = consumes(segToDuring);
    const auto atT2 = consumes(segToAfter);
    INFO(
        "consume lines: " << atT1.size() << " crossing t1, " << atT2.size()
                          << " crossing t2");
    REQUIRE_FALSE(atT1.empty());
    REQUIRE_FALSE(atT2.empty());
    for(const auto& c : atT1)
      CHECK(c.full == 0);
    for(const auto& c : atT2)
      CHECK(c.full == 0);
    CHECK(atT1.back().newN > atT1.front().oldN); // the process's edge appeared
    CHECK(atT2.back().newN < atT2.front().oldN); // and disappeared again
  }
}
