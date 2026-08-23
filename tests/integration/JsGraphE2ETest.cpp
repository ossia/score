// End-to-end video graphs built the way a user's automation builds them: through
// score's JS scripting API, in a real ossia-score process, asserted on the pixels
// that come out of the window readback.
//
//   file -> filter -> output    the decode path, wired from script. The clip is
//                               lossless RGBA written by this test, so "score
//                               decoded it correctly" is an equality, not a PSNR.
//   fan-out to two outputs      one source, two independent window devices. The
//                               failures this catches are a second output that
//                               renders black, and both outputs sharing one
//                               render list.
//   build / play / stop / tear  the same graph created and destroyed several
//   down, repeatedly            times in one process: renders correctly on the
//                               LAST cycle, and the process still exits 0.
//
// Every case asserts the frame-numbered pattern from MovingPattern.hpp at 1:1 and
// requires the frame numbers to rise, for the same reason as
// PipewireVideoMotionTest.
//
// Recipe notes kept on purpose:
//   * device addresses must be "name:/" -- a bare "name:" fails to parse with no
//     error and no log, and the window stays black;
//   * processes go on Score.rootInterval() directly; a Scenario created from
//     script ends up nested and never executes;
//   * Score.play() from --script works, but only after the document exists;
//   * a null return from Score.* does not throw, so every step is null-checked
//     and reports its own exit code.

#include "MovingPattern.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

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

constexpr int kClipFps = 10;
constexpr int kClipFrames = 300;

const char* kUuidIsf = "74ca45ff-92c9-44a0-8f1a-754dea05ee1b";
const char* kUuidVideo = "32dc5341-7748-4c31-a226-82e6bd685744";
const char* kUuidWindow = "5a181207-7d40-4ad8-814e-879fcdf8cc31";

//! A lossless RGBA clip of the frame-numbered pattern. rawvideo in NUT is the
//! only combination that carries the bytes through untouched: every other
//! muxer converts, and then the test would be asserting ffmpeg's conversion
//! instead of score's decode.
bool writeClip(const QString& rawPath, const QString& clipPath)
{
  // SCORE_TEST_PATTERN_NEGATIVE mutates the CLIP, never the assertions: see
  // MovingPattern::Mutation. `frozen` must turn the motion checks red and
  // leave the content ones green, `wrong` the other way round.
  const auto mut = MovingPattern::mutationFromEnvironment();
  if(mut != MovingPattern::Mutation::None)
    WARN("SCORE_TEST_PATTERN_NEGATIVE is set: this run is a negative control "
         "and is EXPECTED to fail");
  if(!MovingPattern::writeRawFrames(rawPath, kClipFrames, mut))
    return false;

  QProcess ff;
  ff.setProcessChannelMode(QProcess::MergedChannels);
  ff.start(
      "ffmpeg",
      {"-nostdin", "-loglevel", "error", "-y", "-f", "rawvideo", "-pix_fmt",
       "rgba", "-s",
       QString::number(MovingPattern::kWidth) + "x"
           + QString::number(MovingPattern::kHeight),
       "-r", QString::number(kClipFps), "-i", rawPath, "-c:v", "rawvideo", "-f",
       "nut", clipPath});
  if(!ff.waitForStarted(10000) || !ff.waitForFinished(120000))
    return false;
  return ff.exitCode() == 0 && QFile::exists(clipPath);
}

struct Run
{
  int exitCode{-1};
  bool crashed{true};
  QString log;
};

Run runScript(const QString& js)
{
  auto env = QProcessEnvironment::systemEnvironment();
  // The window device must render offscreen: with a mapped window the grab
  // reads the SCREEN at its geometry, i.e. the desktop, which is never blank
  // and would sail through any non-blankness check.
  env.insert("SCORE_FORCE_OFFSCREEN_WINDOW", "Window,WindowA,WindowB");
  env.insert("SCORE_AUDIO_BACKEND", "dummy");
  env.insert("SCORE_DISABLE_AUDIOPLUGINS", "1");
  env.insert("QSG_RHI_BACKEND", qEnvironmentVariable("SCORE_TEST_API", "opengl"));
  env.remove("QT_QPA_PLATFORM");

  QProcess p;
  p.setProcessEnvironment(env);
  p.setProcessChannelMode(QProcess::MergedChannels);
  p.start(
      appBinary(), {"--no-gui", "--no-restore", "--script", js, "--wait", "0"});

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
  return path;
}

QStringList existing(const QString& stem, int n)
{
  QStringList out;
  for(int i = 0; i < n; i++)
    if(const QString f = stem + QString::number(i) + ".png"; QFile::exists(f))
      out.push_back(f);
  return out;
}

struct Verdict
{
  std::size_t first{0};   //!< index of the first non-flat grab
  int exact{0};           //!< grabs that matched their own frame exactly
  int checked{0};         //!< grabs from `first` on
  int worstMismatch{0};
  std::vector<int> idx;
  QString detail;
};

Verdict verify(const QStringList& files)
{
  Verdict v;
  std::vector<MovingPattern::Reading> readings;
  for(const auto& f : files)
    readings.push_back(MovingPattern::readFile(f));

  for(std::size_t i = 0; i < readings.size(); i++)
    v.detail += QStringLiteral("grab %1: %2 frame=%3 sampled=%4 mismatched=%5%6\n")
                    .arg(i)
                    .arg(readings[i].uniform ? "flat" : "picture")
                    .arg(readings[i].frame)
                    .arg(readings[i].sampled)
                    .arg(readings[i].mismatched)
                    .arg(readings[i].flippedWouldMatch ? " VERTICALLY-FLIPPED" : "");

  while(v.first < readings.size() && readings[v.first].uniform)
    v.first++;
  for(std::size_t i = v.first; i < readings.size(); i++)
  {
    v.checked++;
    if(readings[i].exact())
      v.exact++;
    else
      v.worstMismatch = std::max(v.worstMismatch, readings[i].mismatched);
    v.idx.push_back(readings[i].frame);
  }
  // Catch2 swallows INFO on success, and "how many pixels did it really
  // compare" is exactly what a green run needs to be able to state.
  if(qEnvironmentVariableIsSet("SCORE_TEST_KEEP_ARTIFACTS"))
    WARN(v.detail.toStdString());
  return v;
}

//! SCORE_TEST_KEEP_ARTIFACTS=1 leaves the script, the clip and every grab on
//! disk: a pixel failure is not diagnosable from the numbers alone.
void keepArtifacts(QTemporaryDir& dir)
{
  if(qEnvironmentVariableIsSet("SCORE_TEST_KEEP_ARTIFACTS"))
  {
    dir.setAutoRemove(false);
    WARN("artifacts kept in " << dir.path().toStdString());
  }
}

//! Common preconditions. A missing display or binary is a failure here, not a
//! skip: this is the only end-to-end cover these graphs have.
void requireEnvironment()
{
  REQUIRE_FALSE(appBinary().isEmpty());
  REQUIRE(QFile::exists(appBinary()));
  REQUIRE(QFile::exists(corpusDir() + "/isf-passthrough-plain.fs"));
  INFO("needs a real display: the offscreen QPA resolves to the Null RHI, "
       "which renders a stable, reproducible, wrong picture");
  // DISPLAY and WAYLAND_DISPLAY answer "is there a window server" only where the
  // window server is X11 or Wayland. Windows and macOS have a native one in any
  // user session and never set either, so requiring them there fails on a real
  // desktop. What matters everywhere is that we are not on the offscreen QPA.
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
  REQUIRE(
      (qEnvironmentVariableIsSet("DISPLAY")
       || qEnvironmentVariableIsSet("WAYLAND_DISPLAY")));
#endif
  REQUIRE(qEnvironmentVariable("QT_QPA_PLATFORM") != "offscreen");
}
}

TEST_CASE(
    "a scripted video file -> filter -> window graph renders the clip",
    "[integration][gfx][js][media]")
{
  requireEnvironment();

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  keepArtifacts(dir);
  const QString clip = dir.filePath("pattern.nut");
  INFO("ffmpeg must be able to mux rawvideo into NUT");
  REQUIRE(writeClip(dir.filePath("frames.rgba"), clip));

  constexpr int kGrabs = 10;
  const QString stem = dir.filePath("grab");
  QString src;
  src += QStringLiteral("var UUID_VIDEO = \"%1\";\n").arg(kUuidVideo);
  src += QStringLiteral("var UUID_ISF = \"%1\";\n").arg(kUuidIsf);
  src += QStringLiteral("var UUID_WINDOW = \"%1\";\n").arg(kUuidWindow);
  src += "Score.createDevice(\"Window\", UUID_WINDOW, {});\n";
  src += "var s = Score.find(\"Scenario.1\"); if (s) Score.remove(s);\n";
  src += "var root = Score.rootInterval();\n";
  src += "var vid = Score.createProcess(root, UUID_VIDEO, \"" + clip + "\");\n";
  src += "if (!vid) { console.log(\"SCENE-ERROR: no video process\"); Qt.exit(9); }\n";
  src += "vid.scaleMode = 3;\n";
  src += "vid.playbackMode = 2;\n";
  src += "console.log(\"SCALEMODE \" + vid.scaleMode + \" PLAYBACK \" + vid.playbackMode);\n";
  src += "var flt = Score.createProcess(root, UUID_ISF, \"" + corpusDir()
         + "/isf-passthrough-plain.fs\");\n";
  src += "if (!flt) { console.log(\"SCENE-ERROR: no filter\"); Qt.exit(10); }\n";
  src += "var c = Score.createCable(Score.outlet(vid, 0), Score.inlet(flt, 0));\n";
  src += "if (!c) { console.log(\"SCENE-ERROR: no cable\"); Qt.exit(11); }\n";
  src += "Score.setAddress(Score.outlet(flt, 0), \"Window:/\");\n";
  src += "var dev = Score.device(\"Window\");\n";
  src += "if (!dev) { console.log(\"SCENE-ERROR: no window device\"); Qt.exit(12); }\n";
  src += "Score.play();\n";
  src += QStringLiteral("for (var i = 0; i < %1; i++) {\n").arg(kGrabs);
  src += "  var t0 = Date.now(); while (Date.now() - t0 < 400) {}\n";
  src += "  dev.grabFrame(2, \"" + stem + "\" + i + \".png\");\n";
  src += "}\n";
  src += "console.log(\"SCENE-OK\");\n";
  src += "Qt.exit(0);\n";

  auto r = runScript(writeScript(dir, "video.js", src));
  INFO(r.log.toStdString());
  CHECK_FALSE(r.crashed);
  CHECK(r.exitCode == 0);
  REQUIRE(r.log.contains("SCENE-OK"));

  const auto files = existing(stem, kGrabs);
  REQUIRE_FALSE(files.isEmpty());
  const auto v = verify(files);
  INFO(v.detail.toStdString());
  INFO("first picture at grab " << v.first << " of " << files.size()
                                << "; worst mismatch " << v.worstMismatch);
  REQUIRE(v.first <= 3);
  REQUIRE(v.checked >= 5);
  REQUIRE(v.exact == v.checked);
  REQUIRE(v.idx.back() > v.idx.front());
}

TEST_CASE(
    "a scripted graph feeds two window outputs at once",
    "[integration][gfx][js][media]")
{
  requireEnvironment();

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  keepArtifacts(dir);
  const QString clip = dir.filePath("pattern.nut");
  REQUIRE(writeClip(dir.filePath("frames.rgba"), clip));

  constexpr int kGrabs = 8;
  const QString stemA = dir.filePath("a");
  const QString stemB = dir.filePath("b");
  QString src;
  src += QStringLiteral("var UUID_VIDEO = \"%1\";\n").arg(kUuidVideo);
  src += QStringLiteral("var UUID_ISF = \"%1\";\n").arg(kUuidIsf);
  src += QStringLiteral("var UUID_WINDOW = \"%1\";\n").arg(kUuidWindow);
  src += "Score.createDevice(\"WindowA\", UUID_WINDOW, {});\n";
  src += "Score.createDevice(\"WindowB\", UUID_WINDOW, {});\n";
  src += "var s = Score.find(\"Scenario.1\"); if (s) Score.remove(s);\n";
  src += "var root = Score.rootInterval();\n";
  src += "var vid = Score.createProcess(root, UUID_VIDEO, \"" + clip + "\");\n";
  src += "if (!vid) { console.log(\"SCENE-ERROR: no video process\"); Qt.exit(9); }\n";
  src += "vid.scaleMode = 3;\n";
  src += "vid.playbackMode = 2;\n";
  src += "console.log(\"SCALEMODE \" + vid.scaleMode + \" PLAYBACK \" + vid.playbackMode);\n";
  src += "var fa = Score.createProcess(root, UUID_ISF, \"" + corpusDir()
         + "/isf-passthrough-plain.fs\");\n";
  src += "var fb = Score.createProcess(root, UUID_ISF, \"" + corpusDir()
         + "/isf-passthrough-plain.fs\");\n";
  src += "if (!fa || !fb) { console.log(\"SCENE-ERROR: no filters\"); Qt.exit(10); }\n";
  // One producer, two consumers: the fan-out is the point of the case.
  src += "var ca = Score.createCable(Score.outlet(vid, 0), Score.inlet(fa, 0));\n";
  src += "var cb = Score.createCable(Score.outlet(vid, 0), Score.inlet(fb, 0));\n";
  src += "if (!ca || !cb) { console.log(\"SCENE-ERROR: no cables\"); Qt.exit(11); }\n";
  src += "Score.setAddress(Score.outlet(fa, 0), \"WindowA:/\");\n";
  src += "Score.setAddress(Score.outlet(fb, 0), \"WindowB:/\");\n";
  src += "var da = Score.device(\"WindowA\"), db = Score.device(\"WindowB\");\n";
  src += "if (!da || !db) { console.log(\"SCENE-ERROR: no window devices\"); Qt.exit(12); }\n";
  src += "Score.play();\n";
  src += QStringLiteral("for (var i = 0; i < %1; i++) {\n").arg(kGrabs);
  src += "  var t0 = Date.now(); while (Date.now() - t0 < 400) {}\n";
  src += "  da.grabFrame(2, \"" + stemA + "\" + i + \".png\");\n";
  src += "  db.grabFrame(0, \"" + stemB + "\" + i + \".png\");\n";
  src += "}\n";
  src += "console.log(\"SCENE-OK\");\n";
  src += "Qt.exit(0);\n";

  auto r = runScript(writeScript(dir, "two.js", src));
  INFO(r.log.toStdString());
  CHECK_FALSE(r.crashed);
  CHECK(r.exitCode == 0);
  REQUIRE(r.log.contains("SCENE-OK"));

  const auto fa = existing(stemA, kGrabs);
  const auto fb = existing(stemB, kGrabs);
  REQUIRE_FALSE(fa.isEmpty());
  REQUIRE_FALSE(fb.isEmpty());

  const auto va = verify(fa);
  const auto vb = verify(fb);
  INFO("WindowA:\n" << va.detail.toStdString());
  INFO("WindowB:\n" << vb.detail.toStdString());
  // Both outputs, independently: a second device that renders black is the
  // failure here, and black is a perfectly plausible-looking window.
  REQUIRE(va.first <= 3);
  REQUIRE(vb.first <= 3);
  REQUIRE(va.checked >= 4);
  REQUIRE(vb.checked >= 4);
  REQUIRE(va.exact == va.checked);
  REQUIRE(vb.exact == vb.checked);
  REQUIRE(va.idx.back() > va.idx.front());
  REQUIRE(vb.idx.back() > vb.idx.front());
}

TEST_CASE(
    "a scripted graph survives being built, played, stopped and torn down "
    "repeatedly",
    "[integration][gfx][js][media]")
{
  requireEnvironment();

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  keepArtifacts(dir);
  const QString clip = dir.filePath("pattern.nut");
  REQUIRE(writeClip(dir.filePath("frames.rgba"), clip));

  constexpr int kCycles = 4;
  constexpr int kGrabsPerCycle = 3;
  const QString stem = dir.filePath("cycle");
  QString src;
  src += QStringLiteral("var UUID_VIDEO = \"%1\";\n").arg(kUuidVideo);
  src += QStringLiteral("var UUID_ISF = \"%1\";\n").arg(kUuidIsf);
  src += QStringLiteral("var UUID_WINDOW = \"%1\";\n").arg(kUuidWindow);
  src += "var s = Score.find(\"Scenario.1\"); if (s) Score.remove(s);\n";
  src += QStringLiteral("for (var cy = 0; cy < %1; cy++) {\n").arg(kCycles);
  src += "  Score.createDevice(\"Window\", UUID_WINDOW, {});\n";
  src += "  var root = Score.rootInterval();\n";
  src += "  var vid = Score.createProcess(root, UUID_VIDEO, \"" + clip + "\");\n";
  src += "  var flt = Score.createProcess(root, UUID_ISF, \"" + corpusDir()
         + "/isf-passthrough-plain.fs\");\n";
  src += "  if (!vid || !flt) { console.log(\"SCENE-ERROR: cycle \" + cy + "
         "\" processes\"); Qt.exit(9); }\n";
  src += "  vid.scaleMode = 3;\n";
  src += "  vid.playbackMode = 2;\n";
  src += "  var c = Score.createCable(Score.outlet(vid, 0), Score.inlet(flt, 0));\n";
  src += "  if (!c) { console.log(\"SCENE-ERROR: cycle \" + cy + \" cable\"); "
         "Qt.exit(10); }\n";
  src += "  Score.setAddress(Score.outlet(flt, 0), \"Window:/\");\n";
  src += "  var dev = Score.device(\"Window\");\n";
  src += "  if (!dev) { console.log(\"SCENE-ERROR: cycle \" + cy + \" device\"); "
         "Qt.exit(11); }\n";
  src += "  Score.play();\n";
  src += QStringLiteral("  for (var i = 0; i < %1; i++) {\n").arg(kGrabsPerCycle);
  src += "    var t0 = Date.now(); while (Date.now() - t0 < 400) {}\n";
  src += "    dev.grabFrame(2, \"" + stem + "\" + cy + \"_\" + i + \".png\");\n";
  src += "  }\n";
  src += "  Score.stop();\n";
  src += "  Score.remove(flt);\n";
  src += "  Score.remove(vid);\n";
  src += "  Score.removeDevice(\"Window\");\n";
  src += "  console.log(\"CYCLE-DONE \" + cy);\n";
  src += "}\n";
  src += "console.log(\"SCENE-OK\");\n";
  src += "Qt.exit(0);\n";

  auto r = runScript(writeScript(dir, "cycles.js", src));
  INFO(r.log.toStdString());
  // Teardown crashes are the failure mode here: the graph, the render lists and
  // the offscreen device all have to come apart cleanly, four times over.
  CHECK_FALSE(r.crashed);
  CHECK(r.exitCode == 0);
  REQUIRE(r.log.contains("SCENE-OK"));
  for(int cy = 0; cy < kCycles; cy++)
    CHECK(r.log.contains(QStringLiteral("CYCLE-DONE %1").arg(cy)));

  // The LAST cycle is the one that matters: a graph that only renders the first
  // time it is built is the regression this case exists for.
  QStringList last;
  for(int i = 0; i < kGrabsPerCycle; i++)
  {
    const QString f
        = stem + QString::number(kCycles - 1) + "_" + QString::number(i) + ".png";
    if(QFile::exists(f))
      last.push_back(f);
  }
  REQUIRE(last.size() == kGrabsPerCycle);
  const auto v = verify(last);
  INFO("last cycle:\n" << v.detail.toStdString());
  REQUIRE(v.first == 0);
  REQUIRE(v.checked == kGrabsPerCycle);
  REQUIRE(v.exact == v.checked);
  REQUIRE(v.idx.back() > v.idx.front());
}
