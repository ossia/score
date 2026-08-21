// A PipeWire video input has to deliver a LIVE picture, and this is the test
// that can tell the difference between one and a plausible still.
//
// Media_PipewireVideoInput asserts that videotestsrc's colour bars come out of
// the window readback. videotestsrc's smpte pattern is a static image, so that
// assertion also passes on a stream that delivered one buffer and stopped, on a
// texture uploaded once and never refreshed, and on a readback serving the same
// cached frame to every grab.
//
// So the source here is a stream whose every frame says which frame it is (see
// MovingPattern.hpp), and the assertions are:
//
//   1. every grabbed picture matches its own frame, pixel for pixel, with no
//      tolerance -- the palette is built from 0/255 channels precisely so that
//      equality is the right operator;
//   2. the frame numbers rise. A frozen texture, a repeated buffer and a stale
//      readback all keep reporting the same number.
//
// The producer is gst-launch replaying bytes written by writeRawFrames(), and
// the expected pixels are computed by the same function, in-process, from the
// frame number the picture carries -- so the reference comes from neither
// score's format tables nor its enumerator. Producer and consumer built from one
// shared format triple can agree with each other about a picture neither is
// really moving.
//
// Prove the assertions are load-bearing (both must go RED):
//   SCORE_TEST_PATTERN_NEGATIVE=frozen ctest -R Media_PipewireVideoMotion
//   SCORE_TEST_PATTERN_NEGATIVE=wrong  ctest -R Media_PipewireVideoMotion

#include "MovingPattern.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QThread>

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

//! 10 fps over 30 s of media: slow enough that a grab lands squarely inside one
//! frame rather than between two, and long enough that the run never reaches
//! the end of the file (where the frame number would restart and break the
//! "rises" assertion for a reason that is not a defect).
constexpr int kProducerFps = 10;
constexpr int kProducerFrames = 300;
constexpr int kGrabs = 12;
constexpr int kGrabSpacingMs = 400;

//! The frame the producer is publishing, replayed from a file this test wrote.
//! Deliberately NOT built to match what score will ask for.
struct Producer
{
  QProcess proc;
  QString node;
  QString log;

  ~Producer() { stop(); }

  bool start(const QString& rawPath, const QString& nodeName)
  {
    node = nodeName;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(
        "gst-launch-1.0",
        {"-q", "multifilesrc", "location=" + rawPath, "loop=false", "!",
         "rawvideoparse", "format=rgba",
         "width=" + QString::number(MovingPattern::kWidth),
         "height=" + QString::number(MovingPattern::kHeight),
         "framerate=" + QString::number(kProducerFps) + "/1", "!", "pipewiresink",
         "mode=provide",
         "stream-properties=p,media.class=Video/Source,node.name=" + node});
    if(!proc.waitForStarted(10000))
      return false;

    // The node has to be visible in the graph before score is told to connect
    // to it by name; there is no retry on the device side.
    for(int i = 0; i < 20; i++)
    {
      QThread::msleep(500);
      QProcess dump;
      dump.start("pw-dump", {});
      if(dump.waitForFinished(8000)
         && QString::fromUtf8(dump.readAllStandardOutput()).contains(node))
        return true;
      if(proc.state() != QProcess::Running)
        break;
    }
    log = QString::fromUtf8(proc.readAll());
    return false;
  }

  void stop()
  {
    if(proc.state() == QProcess::NotRunning)
      return;
    proc.terminate();
    if(!proc.waitForFinished(5000))
    {
      proc.kill();
      proc.waitForFinished(2000);
    }
  }
};

struct Run
{
  int exitCode{-1};
  bool crashed{true};
  QString log;
  QStringList frames;
};

//! One score process: build pipewire-in -> ISF passthrough -> offscreen window,
//! play, and grab repeatedly with a real gap in between so that a stream which
//! is actually running has time to advance.
Run runScore(const QTemporaryDir& dir, const QString& node)
{
  const QString js = dir.filePath("scene.js");
  const QString stem = dir.filePath("grab");

  QString src;
  src += "var UUID_PWIN   = \"cf6a355f-34d1-4d24-a6ea-3d204f93cde9\";\n";
  src += "var UUID_ISF    = \"74ca45ff-92c9-44a0-8f1a-754dea05ee1b\";\n";
  src += "var UUID_WINDOW = \"5a181207-7d40-4ad8-814e-879fcdf8cc31\";\n";
  // Verbatim what Gfx::PipeWire's enumerator writes into every discovered
  // node's path. The producer publishes something else entirely; a device that
  // holds out for its request renders black and fails on the first grab.
  src += "Score.createDevice(\"pwin\", UUID_PWIN, { Path: \"pipewire://" + node
         + "?width=1920&height=1080&fps=30&format=rgba\" });\n";
  src += "Score.createDevice(\"Window\", UUID_WINDOW, {});\n";
  src += "var s = Score.find(\"Scenario.1\"); if (s) Score.remove(s);\n";
  // Processes go on the root interval: a scenario created from script ends up
  // nested and never executes.
  src += "var proc = Score.createProcess(Score.rootInterval(), UUID_ISF, \""
         + corpusDir() + "/isf-passthrough-plain.fs\");\n";
  src += "if (!proc) { console.log(\"SCENE-ERROR: no process\"); Qt.exit(9); }\n";
  src += "var inl = Score.inlet(proc, 0);\n";
  src += "if (!inl) { console.log(\"SCENE-ERROR: no image inlet\"); Qt.exit(10); }\n";
  src += "Score.setAddress(inl, \"pwin:/\");\n";
  src += "Score.setAddress(Score.outlet(proc, 0), \"Window:/\");\n";
  src += "var dev = Score.device(\"Window\");\n";
  src += "if (!dev) { console.log(\"SCENE-ERROR: no window device\"); Qt.exit(11); }\n";
  src += "Score.play();\n";
  // The script engine owns the main thread, so the gap has to be a spin; the
  // pipewire loop fills the input pool from its own thread meanwhile.
  src += "for (var i = 0; i < " + QString::number(kGrabs) + "; i++) {\n";
  src += "  var t0 = Date.now(); while (Date.now() - t0 < "
         + QString::number(kGrabSpacingMs) + ") {}\n";
  src += "  dev.grabFrame(2, \"" + stem + "\" + i + \".png\");\n";
  src += "}\n";
  src += "console.log(\"SCENE-OK\");\n";
  src += "Qt.exit(0);\n";

  {
    QFile f{js};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(src.toUtf8());
  }

  auto env = QProcessEnvironment::systemEnvironment();
  // Without this the Window device maps a real window and the grab captures the
  // SCREEN at its geometry -- the desktop, which is never blank and would pass
  // a blankness check while showing nothing of the render.
  env.insert("SCORE_FORCE_OFFSCREEN_WINDOW", "Window");
  env.insert("SCORE_AUDIO_BACKEND", "dummy");
  env.insert("SCORE_DISABLE_AUDIOPLUGINS", "1");
  // Pick the backend rather than inheriting whatever the developer last saved
  // in the settings. Not offscreen: QT_QPA_PLATFORM=offscreen resolves to the
  // Null RHI, which writes a stable, reproducible, wrong picture.
  const QString api = qEnvironmentVariable("SCORE_TEST_API", "opengl");
  env.insert("QSG_RHI_BACKEND", api);
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
  for(int i = 0; i < kGrabs; i++)
  {
    const QString f = stem + QString::number(i) + ".png";
    if(QFile::exists(f))
      r.frames.push_back(f);
  }
  return r;
}
}

TEST_CASE(
    "a PipeWire video input delivers moving pixels",
    "[integration][gfx][pipewire][media]")
{
  REQUIRE_FALSE(appBinary().isEmpty());
  REQUIRE(QFile::exists(appBinary()));
  REQUIRE(QFile::exists(corpusDir() + "/isf-passthrough-plain.fs"));

  // A real session is a requirement, not a condition. The offscreen QPA has no
  // GL for the readback and silently falls back to a backend that renders a
  // picture no assertion here should ever be allowed to accept.
  INFO("needs a real display: the offscreen QPA resolves to the Null RHI");
  REQUIRE(
      (qEnvironmentVariableIsSet("DISPLAY")
       || qEnvironmentVariableIsSet("WAYLAND_DISPLAY")));
  REQUIRE(qEnvironmentVariable("QT_QPA_PLATFORM") != "offscreen");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  const auto mutation = MovingPattern::mutationFromEnvironment();
  if(mutation != MovingPattern::Mutation::None)
    WARN("SCORE_TEST_PATTERN_NEGATIVE is set: this run is a negative control "
         "and is EXPECTED to fail");

  const QString raw = dir.filePath("frames.rgba");
  REQUIRE(MovingPattern::writeRawFrames(raw, kProducerFrames, mutation));

  Producer prod;
  const QString node
      = QStringLiteral("score-motion-%1").arg(QCoreApplication::applicationPid());
  INFO("gst-launch-1.0 must be able to publish a Video/Source; run this test "
       "through score_add_media_test, which requires the media stack");
  INFO(prod.log.toStdString());
  REQUIRE(prod.start(raw, node));

  auto r = runScore(dir, node);
  prod.stop();

  INFO(r.log.toStdString());
  CHECK_FALSE(r.crashed);
  CHECK(r.exitCode == 0);
  CHECK(r.log.contains("SCENE-OK"));
  // Naming the symptom of the negotiation regression: with an exact-match-only
  // advertisement the server cannot intersect the format lists and says so.
  CHECK_FALSE(r.log.contains("no more input formats"));
  REQUIRE_FALSE(r.frames.isEmpty());

  std::vector<MovingPattern::Reading> readings;
  for(const auto& f : r.frames)
    readings.push_back(MovingPattern::readFile(f));

  QString detail;
  for(int i = 0; i < int(readings.size()); i++)
  {
    const auto& rd = readings[i];
    detail += QStringLiteral("grab %1: %2 frame=%3 sampled=%4 mismatched=%5\n")
                  .arg(i)
                  .arg(rd.uniform ? "flat" : "picture")
                  .arg(rd.frame)
                  .arg(rd.sampled)
                  .arg(rd.mismatched);
  }
  INFO(detail.toStdString());

  // The readback before the producer's first buffer lands is a cleared target:
  // one flat colour, which is not a frame of this pattern and is never counted
  // as one. Only a LEADING run of those is startup; a flat picture after the
  // stream has been seen working means it stopped, and that is a failure.
  std::size_t first = 0;
  while(first < readings.size() && readings[first].uniform)
    first++;
  INFO("first picture at grab " << first << " of " << readings.size());
  REQUIRE(first <= 2);

  // --- 1. content, at 1:1 against the pattern this test wrote ---------------
  int exact = 0, worstMismatch = 0, totalSampled = 0;
  for(std::size_t i = first; i < readings.size(); i++)
  {
    const auto& rd = readings[i];
    totalSampled += rd.sampled;
    if(rd.exact())
      exact++;
    else
      worstMismatch = std::max(worstMismatch, rd.mismatched);
  }
  INFO("compared " << totalSampled << " pixels; worst grab had " << worstMismatch
                   << " that were not the exact expected colour");
  REQUIRE(exact == int(readings.size() - first));

  // --- 2. motion ------------------------------------------------------------
  // Every picture is a real frame; now they have to be DIFFERENT real frames,
  // arriving in the order the producer sends them.
  std::vector<int> idx;
  for(std::size_t i = first; i < readings.size(); i++)
    idx.push_back(readings[i].frame);
  REQUIRE(idx.size() >= 8);

  int distinct = 0, regressions = 0;
  for(std::size_t i = 0; i < idx.size(); i++)
  {
    if(i == 0 || idx[i] != idx[i - 1])
      distinct++;
    if(i > 0 && idx[i] < idx[i - 1])
      regressions++;
  }
  QString seq;
  for(int k : idx)
    seq += QString::number(k) + " ";
  INFO("frame indices: " << seq.toStdString());
  // A stream that stopped after its first buffer, a texture uploaded once, and
  // a readback serving a cached frame all produce one repeated index here.
  REQUIRE(idx.back() > idx.front());
  REQUIRE(distinct >= 4);
  // Frames never arrive out of order from a file replayed forward: a lower
  // index than the previous grab means the consumer went back to an older
  // buffer, which is the frame-repeat failure the roundtrip harness counts.
  CHECK(regressions == 0);
  // The producer is publishing at a known rate. Over kGrabs grabs spaced
  // kGrabSpacingMs apart the stream advances by at most that many frames plus
  // slack for the app's own startup; an index that ran far past that came from
  // somewhere other than this producer.
  const int maxPlausible
      = kProducerFps * (kGrabs * kGrabSpacingMs + 60000) / 1000;
  CHECK(idx.back() <= maxPlausible);
}
