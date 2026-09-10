// score's PipeWire video OUTPUT, driven the way a user drives it: the real
// application, a real device, playing, with an outside consumer attached.
//
// The harness in src/plugins/score-plugin-gfx/tests/PipewireRoundtrip.cpp
// covers the producer, but it drives the output node from a QTimer of its own.
// Everything about pacing therefore comes from the harness rather than from the
// application, which is exactly what an OBS session complains about. This one
// starts `ossia-score --script`, lets the graphics context clock the node, and
// measures what a GStreamer consumer actually receives:
//
//   * the pixels: an ISF gradient, dark on the left and bright on the right, so
//     a frame that arrives blank, torn, or mirrored is not a pass;
//   * the rate: frames per second against the rate the device asked for. A
//     consumer that receives correct frames at a tenth of the requested rate is
//     the bug this file exists for, and it cannot be seen by a pixel check.
//
// Needs a PipeWire session, gst-launch-1.0 and a real display. Skips saying
// which is missing rather than passing on nothing.

#include <QByteArray>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QSet>
#include <QTemporaryDir>
#include <QThread>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <optional>

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

bool haveTool(const QString& tool, const QStringList& args = {"--version"})
{
  QProcess p;
  p.start(tool, args);
  return p.waitForFinished(5000) && p.exitCode() == 0;
}

//! The requested geometry and rate. Small enough to be cheap, big enough that a
//! per-frame copy of it is not free.
constexpr int kWidth = 640;
constexpr int kHeight = 360;
constexpr double kRate = 60.;

QString write(const QTemporaryDir& dir, const QString& name, const QString& body)
{
  const QString path = dir.filePath(name);
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(body.toUtf8());
  f.close();
  return path;
}

//! Builds the document: one ISF gradient on the root interval, addressed to a
//! PipeWire output device, playing. It never exits -- the test kills the child.
QString sceneScript(
    const QString& nodeName, bool dmabuf, const char* shader = "isf-gradient-x.fs",
    int w = kWidth, int h = kHeight)
{
  QString path = nodeName + "?format=rgba";
  if(dmabuf)
    path += "&dmabuf=on";

  QString src;
  src += "var UUID_ISF = \"74ca45ff-92c9-44a0-8f1a-754dea05ee1b\";\n";
  src += "var UUID_PWOUT = \"d5e7b22b-b7f6-4680-9610-2457509b7946\";\n";
  // The keys are the settings' own JSON names, not the C++ members: "Path",
  // "Width", "Height", "Rate". Getting them wrong is silent -- the device comes
  // up at 1x1 and publishes four-byte frames.
  src += QStringLiteral(
             "Score.createDevice(\"PWOut\", UUID_PWOUT, {\"Path\": \"%1\", "
             "\"Width\": %2, \"Height\": %3, \"Rate\": %4});\n")
             .arg(path)
             .arg(w)
             .arg(h)
             .arg(kRate);
  src += "var s = Score.find(\"Scenario.1\"); if (s) Score.remove(s);\n";
  src += "var root = Score.rootInterval();\n";
  src += "var flt = Score.createProcess(root, UUID_ISF, \"" + corpusDir() + "/"
         + QString::fromUtf8(shader) + "\");\n";
  src += "if (!flt) { console.log(\"SCENE-ERROR: no filter\"); Qt.exit(9); }\n";
  src += "Score.setAddress(Score.outlet(flt, 0), \"PWOut:/\");\n";
  src += "Score.play();\n";
  src += "console.log(\"SCENE-OK\");\n";
  return src;
}

//! The publishing application, kept alive for the length of a measurement.
struct Publisher
{
  QProcess proc;
  QString log;

  void start(const QString& js)
  {
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("SCORE_AUDIO_BACKEND", "dummy");
    env.insert("SCORE_DISABLE_AUDIOPLUGINS", "1");
    env.insert("QT_FORCE_STDERR_LOGGING", "1");
    env.insert("QT_ASSUME_STDERR_HAS_CONSOLE", "1");
    proc.setProcessEnvironment(env);
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(
        appBinary(), {"--no-gui", "--no-restore", "--wait", "0", "--script", js});
  }

  ~Publisher()
  {
    if(proc.state() != QProcess::NotRunning)
    {
      proc.terminate();
      if(!proc.waitForFinished(5000))
        proc.kill();
      proc.waitForFinished(2000);
    }
    log += QString::fromUtf8(proc.readAll());
  }
};

//! object.serial of the (single) live node with this name, or -1. The serial,
//! not the id: ids are recycled and a stale one resolves to somebody else.
int64_t nodeSerial(const QString& name)
{
  QProcess dump;
  dump.start("pw-dump", {});
  if(!dump.waitForFinished(6000))
    return -1;
  const auto doc = QJsonDocument::fromJson(dump.readAllStandardOutput());
  if(!doc.isArray())
    return -1;

  int64_t serial = -1;
  for(const auto& v : doc.array())
  {
    const auto o = v.toObject();
    if(o.value("type").toString() != "PipeWire:Interface:Node")
      continue;
    const auto props = o.value("info").toObject().value("props").toObject();
    if(props.value("node.name").toString() != name)
      continue;
    serial = std::max<int64_t>(serial, props.value("object.serial").toInteger(-1));
  }
  return serial;
}

//! Wait for the application to publish, then return its serial.
int64_t waitForNode(const QString& name, int timeoutMs)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < timeoutMs)
  {
    if(const auto s = nodeSerial(name); s >= 0)
      return s;
    QThread::msleep(250);
  }
  return -1;
}

struct Capture
{
  int frames{};
  int distinct{};
  double seconds{};
  QStringList files;
  QString log;

  double fps() const { return seconds > 0. ? frames / seconds : 0.; }
  double distinctFps() const { return seconds > 0. ? distinct / seconds : 0.; }
};

//! Read `count` buffers off the node and write them as PNGs. The wall time
//! covers only the buffers, not the pipeline setup: num-buffers makes
//! gst-launch exit as soon as it has them.
Capture capture(
    int64_t serial, int count, const QTemporaryDir& dir, const QString& stem,
    bool dmabuf)
{
  Capture c;
  QStringList args{
      "pipewiresrc", QStringLiteral("target-object=%1").arg(serial),
      QStringLiteral("num-buffers=%1").arg(count)};
  if(dmabuf)
  {
    // Ask for DMA-BUF memory explicitly, so the pipeline fails to negotiate
    // rather than quietly falling back to a copy -- that is the whole point of
    // the case. videoconvert cannot read that memory, so the frame comes back
    // through GL: upload the imported buffer, download it to host memory, and
    // only then convert.
    args << "!" << "video/x-raw(memory:DMABuf)" << "!" << "glupload" << "!"
         << "gldownload";
  }
  args << "!" << "videoconvert" << "!" << "video/x-raw,format=RGB" << "!"
       << "pngenc" << "!" << "multifilesink"
       << QStringLiteral("location=%1/%2_%d.png").arg(dir.path(), stem);

  QProcess gst;
  gst.setProcessChannelMode(QProcess::MergedChannels);
  QElapsedTimer t;
  t.start();
  gst.start("gst-launch-1.0", args);
  if(!gst.waitForStarted(10000) || !gst.waitForFinished(60000))
  {
    gst.kill();
    gst.waitForFinished(2000);
  }
  c.seconds = t.elapsed() / 1000.;
  c.log = QString::fromUtf8(gst.readAll());

  for(int i = 0; i < count; i++)
  {
    const QString f = QStringLiteral("%1/%2_%3.png").arg(dir.path(), stem).arg(i);
    if(QFile::exists(f))
      c.files.push_back(f);
  }
  c.frames = c.files.size();

  // Distinct pictures, not distinct buffers. A producer that hands the same
  // frame over six times keeps a buffer counter happy and still looks frozen,
  // so the rate that matters is how many DIFFERENT frames arrive.
  QSet<QByteArray> seen;
  for(const auto& f : c.files)
  {
    QFile fh{f};
    if(!fh.open(QIODevice::ReadOnly))
      continue;
    seen.insert(QCryptographicHash::hash(fh.readAll(), QCryptographicHash::Md5));
  }
  c.distinct = seen.size();
  return c;
}

//! isf-gradient-x paints red = x: dark at the left edge, bright at the right.
//! Checks the picture is that gradient, at the requested geometry.
struct PixelVerdict
{
  bool ok{};
  QString why;
};

PixelVerdict verifyGradient(const QString& png)
{
  QImage img{png};
  if(img.isNull())
    return {false, "unreadable png"};
  if(img.width() != kWidth || img.height() != kHeight)
    return {false,
            QStringLiteral("geometry %1x%2").arg(img.width()).arg(img.height())};

  const int y = img.height() / 2;
  const int left = qRed(img.pixel(4, y));
  const int right = qRed(img.pixel(img.width() - 5, y));
  const int mid = qRed(img.pixel(img.width() / 2, y));

  if(right - left < 100)
    return {false,
            QStringLiteral("not a gradient: left %1 mid %2 right %3")
                .arg(left)
                .arg(mid)
                .arg(right)};
  if(std::abs(mid - (left + right) / 2) > 40)
    return {false, QStringLiteral("middle %1 is off the ramp").arg(mid)};
  return {true, {}};
}

QString environmentGap()
{
  if(appBinary().isEmpty() || !QFile::exists(appBinary()))
    return "the score application binary was not built";
  if(!QFile::exists(corpusDir() + "/isf-gradient-x.fs"))
    return "the gfx corpus is missing";
  if(!haveTool("pw-dump", {"--help"}))
    return "no pw-dump: this needs a PipeWire session";
  if(!haveTool("gst-launch-1.0"))
    return "no gst-launch-1.0";
#if defined(__linux__)
  if(!qEnvironmentVariableIsSet("DISPLAY")
     && !qEnvironmentVariableIsSet("WAYLAND_DISPLAY"))
    return "needs a real display: the offscreen QPA has no GL for the render";
#endif
  if(qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen")
    return "needs a real display: QT_QPA_PLATFORM is offscreen";
  return {};
}

//! A node name nothing else can collide with, so a leftover process from an
//! earlier run cannot be measured by mistake.
QString uniqueNodeName(const char* tag)
{
  return QStringLiteral("score-pwtest-%1-%2")
      .arg(tag)
      .arg(QCoreApplication::applicationPid());
}
}

TEST_CASE(
    "a PipeWire video output delivers its picture to an outside consumer",
    "[integration][gfx][pipewire][media]")
{
  if(const auto gap = environmentGap(); !gap.isEmpty())
    SKIP(gap.toStdString());

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  const QString node = uniqueNodeName("shm");
  Publisher pub;
  pub.start(write(dir, "scene.js", sceneScript(node, false)));

  const auto serial = waitForNode(node, 45000);
  INFO("the application never published a PipeWire node called " << node.toStdString());
  REQUIRE(serial >= 0);

  constexpr int kFrames = 120;
  const auto cap = capture(serial, kFrames, dir, "shm", false);
  INFO(cap.log.toStdString());

  REQUIRE(cap.frames > 0);
  // Every requested buffer, not just the first: a producer that publishes one
  // frame and stops is the shape a still picture in OBS has.
  CHECK(cap.frames == kFrames);

  const auto first = verifyGradient(cap.files.front());
  INFO("first frame: " << first.why.toStdString());
  CHECK(first.ok);

  const auto last = verifyGradient(cap.files.back());
  INFO("last frame: " << last.why.toStdString());
  CHECK(last.ok);
}

// The performance half, and the reason this file exists rather than another
// pixel test. A consumer that receives the right picture at a fraction of the
// requested rate is what "it works but is extremely slow" means, and no pixel
// comparison can see it.
TEST_CASE(
    "a PipeWire video output keeps up with the rate it advertises",
    "[integration][gfx][pipewire][media][perf]")
{
  if(const auto gap = environmentGap(); !gap.isEmpty())
    SKIP(gap.toStdString());

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  const QString node = uniqueNodeName("rate");
  Publisher pub;
  // An ANIMATED source: the gradient never changes, so every frame of it
  // hashes the same and a frozen producer would be indistinguishable from a
  // smooth one.
  pub.start(write(dir, "scene.js", sceneScript(node, false, "isf-time-uniforms.fs")));

  const auto serial = waitForNode(node, 45000);
  REQUIRE(serial >= 0);

  // Warm up first: the first buffers cover negotiation and the consumer's own
  // start-up, and folding those into the measurement understates the rate.
  (void)capture(serial, 20, dir, "warm", false);

  constexpr int kFrames = 180;
  const auto cap = capture(serial, kFrames, dir, "rate", false);
  INFO(cap.log.toStdString());
  REQUIRE(cap.frames == kFrames);

  INFO(
      "received " << cap.frames << " buffers (" << cap.distinct
                  << " distinct pictures) in " << cap.seconds << " s = "
                  << cap.fps() << " buffers/s, " << cap.distinctFps()
                  << " frames/s; the device asked for " << kRate);

  // Half the advertised rate, on both counts. Deliberately loose: this is a
  // shared machine running a real compositor, and the case is here to catch a
  // collapse to a few frames a second, not to police jitter.
  CHECK(cap.fps() >= kRate / 2.);
  CHECK(cap.distinctFps() >= kRate / 2.);
}

// The same measurement at the geometry a user actually sends to OBS. Every
// per-frame copy costs six times what it does at 640x360, so a path that keeps
// up at preview size can still collapse here -- and this is the size the
// complaint arrives at.
TEST_CASE(
    "a PipeWire video output keeps up at 1920x1080",
    "[integration][gfx][pipewire][media][perf]")
{
  if(const auto gap = environmentGap(); !gap.isEmpty())
    SKIP(gap.toStdString());

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  const QString node = uniqueNodeName("hd");
  Publisher pub;
  pub.start(write(
      dir, "scene.js",
      sceneScript(node, false, "isf-time-uniforms.fs", 1920, 1080)));

  const auto serial = waitForNode(node, 45000);
  REQUIRE(serial >= 0);

  (void)capture(serial, 20, dir, "hdwarm", false);

  constexpr int kFrames = 120;
  const auto cap = capture(serial, kFrames, dir, "hd", false);
  INFO(cap.log.toStdString());
  REQUIRE(cap.frames == kFrames);

  INFO(
      "1920x1080: " << cap.frames << " buffers (" << cap.distinct
                    << " distinct) in " << cap.seconds << " s = "
                    << cap.distinctFps() << " frames/s, asked for " << kRate);

  CHECK(cap.distinctFps() >= kRate / 2.);
}

// Zero-copy. score allocates exportable images and hands the consumer their
// DMA-BUF file descriptors instead of copying pixels through host memory, so
// this asks GStreamer for DMA-BUF memory explicitly: the pipeline fails to
// negotiate rather than silently falling back to a copy.
// [!shouldfail]: zero-copy does not negotiate yet. score offers the modifier as
// a DONT_FIXATE choice and answers the consumer's reply with a fixated format
// plus the alternatives, which is the handshake pipewire's own
// video-src-fixate example performs -- and the server still ends the
// negotiation with "no more output formats". The remaining suspect is that no
// modifier survives the intersection here (LINEAR from a Vulkan export against
// what this GStreamer/driver pair will import), in which case the fix is to
// offer host memory alongside dma-buf so the link falls back instead of dying.
// Until then a dmabuf=on device is unusable from outside score, which is the
// black source in OBS.
TEST_CASE(
    "a PipeWire video output can hand over DMA-BUF memory",
    "[integration][gfx][pipewire][media][dmabuf][!shouldfail]")
{
  if(const auto gap = environmentGap(); !gap.isEmpty())
    SKIP(gap.toStdString());

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  const QString node = uniqueNodeName("dmabuf");
  Publisher pub;
  pub.start(write(dir, "scene.js", sceneScript(node, true)));

  const auto serial = waitForNode(node, 45000);
  REQUIRE(serial >= 0);

  constexpr int kFrames = 60;
  const auto cap = capture(serial, kFrames, dir, "dma", true);
  INFO(cap.log.toStdString());

  REQUIRE(cap.frames > 0);
  CHECK(cap.frames == kFrames);

  const auto v = verifyGradient(cap.files.back());
  INFO("dma-buf frame: " << v.why.toStdString());
  CHECK(v.ok);
}
