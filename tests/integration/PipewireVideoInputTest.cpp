// End-to-end proof that a PipeWire video input device delivers a producer's
// pixels through a real score process: device -> video_texture_input_protocol
// -> CameraNode -> ISF passthrough -> Window readback.
//
// Everything below the device layer already had coverage: PipewireRoundtrip's
// pw2s cells drive InputStream directly, with a producer built to match the
// consumer's request exactly and an explicit pw-link. Neither condition holds
// in the application, and the case that broke was precisely the one the
// harness cannot express -- score asking for the geometry its own enumerator
// writes into every device path (1920x1080 RGBA 30fps) from a source that
// publishes something else. The stream advertised that single triple and
// nothing more, the link went to `state error` / "no more input formats", and
// no frame ever arrived while the device still reported itself connected.
//
// So the source here is deliberately NOT what the device asks for: the request
// is score's own default, the producer is the runner's 320x240 node, and the
// assertion is on pixels rather than on frame counts -- a device that
// negotiates nothing renders black, and black is indistinguishable from every
// other failure at this level.

#include <QByteArray>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <string>

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

//! videotestsrc's SMPTE bars, in order, as they arrive after an RGBA
//! round-trip. Saturated primaries survive the texture upload and the
//! passthrough shader unchanged, which is what makes them assertable.
constexpr QRgb kSmpteBars[]{
    qRgb(255, 255, 255), qRgb(255, 255, 0), qRgb(0, 255, 255), qRgb(0, 255, 0),
    qRgb(255, 0, 255),   qRgb(255, 0, 0),   qRgb(0, 0, 255)};

//! The corpus shader tiles the input into a 2x2 grid; the top-left cell is a
//! plain IMG_NORM_PIXEL sample of the whole frame, so the bars span its width.
bool hasSmpteBars(const QString& path)
{
  QImage img{path};
  if(img.isNull())
    return false;
  img = img.convertToFormat(QImage::Format_RGB32);
  const int cellW = img.width() / 2;
  // A fifth of the way down: inside the tall colour bars, above the
  // pluge/noise strips at the bottom of the pattern.
  const int y = img.height() / 10;
  if(cellW < 14 || y <= 0)
    return false;

  int matched = 0;
  for(int bar = 0; bar < 7; bar++)
  {
    const int x = int((bar + 0.5) * cellW / 7.);
    if(img.pixel(x, y) == kSmpteBars[bar])
      matched++;
  }
  return matched == 7;
}

int distinctColours(const QString& path)
{
  QImage img{path};
  if(img.isNull())
    return 0;
  img = img.convertToFormat(QImage::Format_RGB32);
  QSet<QRgb> seen;
  for(int y = 0; y < img.height(); y++)
    for(int x = 0; x < img.width(); x++)
      seen.insert(img.pixel(x, y));
  return seen.size();
}

struct Run
{
  int exitCode{-1};
  bool crashed{true};
  QString log;
  QStringList frames;
};

//! One score process: build the graph, play, and grab repeatedly. Repeatedly
//! because a live producer needs a moment to deliver its first buffer while
//! the window's step clock runs as fast as it can -- a single early grab is
//! blank for a reason that has nothing to do with the device.
Run run(const QTemporaryDir& dir, const QString& node, const QString& request)
{
  const QString js = dir.filePath("scene.js");
  const QString stem = dir.filePath("frame");

  QString src;
  src += "var UUID_PWIN   = \"cf6a355f-34d1-4d24-a6ea-3d204f93cde9\";\n";
  src += "var UUID_ISF    = \"74ca45ff-92c9-44a0-8f1a-754dea05ee1b\";\n";
  src += "var UUID_WINDOW = \"5a181207-7d40-4ad8-814e-879fcdf8cc31\";\n";
  src += "Score.createDevice(\"pwin\", UUID_PWIN, { Path: \"pipewire://" + node
         + request + "\" });\n";
  src += "Score.createDevice(\"Window\", UUID_WINDOW, {});\n";
  src += "var s = Score.find(\"Scenario.1\"); if (s) Score.remove(s);\n";
  src += "var proc = Score.createProcess(Score.rootInterval(), UUID_ISF, \""
         + corpusDir() + "/isf-image-passthrough.fs\");\n";
  src += "if (!proc) { console.log(\"SCENE-ERROR: no process\"); Qt.exit(9); }\n";
  src += "var inl = Score.inlet(proc, 0);\n";
  src += "if (!inl) { console.log(\"SCENE-ERROR: no image inlet\"); Qt.exit(10); }\n";
  src += "Score.setAddress(inl, \"pwin:/\");\n";
  src += "Score.setAddress(Score.outlet(proc, 0), \"Window:/\");\n";
  src += "var dev = Score.device(\"Window\");\n";
  src += "if (!dev) { console.log(\"SCENE-ERROR: no window device\"); Qt.exit(11); }\n";
  src += "Score.play();\n";
  // The script engine owns the main thread here, so the wait has to be a spin:
  // the pipewire loop fills the input pool from its own thread meanwhile.
  src += "for (var i = 0; i < 12; i++) {\n";
  src += "  var t0 = Date.now(); while (Date.now() - t0 < 500) {}\n";
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
  // Otherwise the Window device maps a real window and grabTo/grabFrame falls
  // back to grabbing the desktop, which is never blank and would "pass".
  env.insert("SCORE_FORCE_OFFSCREEN_WINDOW", "Window");
  env.insert("SCORE_AUDIO_BACKEND", "dummy");
  env.insert("SCORE_DISABLE_AUDIOPLUGINS", "1");

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
    return r;
  }
  r.log = QString::fromUtf8(p.readAll());
  r.crashed = p.exitStatus() != QProcess::NormalExit;
  r.exitCode = p.exitCode();
  for(int i = 0; i < 12; i++)
  {
    const QString f = stem + QString::number(i) + ".png";
    if(QFile::exists(f))
      r.frames.push_back(f);
  }
  return r;
}
}

TEST_CASE(
    "a PipeWire video input renders the producer's pixels",
    "[integration][gfx][pipewire][media]")
{
  // The provisioning wrapper (tests/hardware/with-virtual-media.sh --video)
  // is a hard requirement, not a condition: without it this would quietly
  // stop testing anything.
  const QString node = qEnvironmentVariable("SCORE_TEST_PW_VIDEO_NODE");
  INFO("SCORE_TEST_PW_VIDEO_NODE is set by tests/hardware/with-virtual-media.sh "
       "--video; run this test through score_add_media_test, not directly");
  REQUIRE_FALSE(node.isEmpty());
  REQUIRE_FALSE(appBinary().isEmpty());
  REQUIRE(QFile::exists(appBinary()));
  REQUIRE(QFile::exists(corpusDir() + "/isf-image-passthrough.fs"));
  // The offscreen QPA has no GL: the window device aborts before any readback,
  // so this needs a real session rather than a fallback.
  INFO("needs a real display: the offscreen QPA has no GL for the readback");
  REQUIRE(
      (qEnvironmentVariableIsSet("DISPLAY")
       || qEnvironmentVariableIsSet("WAYLAND_DISPLAY")));

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  // Deliberately not the producer's geometry: this is verbatim what
  // Gfx::PipeWire's own enumerator writes into every discovered node's path,
  // and the producer publishes 320x240. Asking for it is the regression this
  // test exists for.
  auto r = run(dir, node, "?width=1920&height=1080&fps=30&format=rgba");

  INFO(r.log.toStdString());
  CHECK_FALSE(r.crashed);
  CHECK(r.exitCode == 0);
  CHECK(r.log.contains("SCENE-OK"));
  // Naming the symptom: with an exact-match-only advertisement the server
  // cannot intersect our format list with the producer's and says so.
  CHECK_FALSE(r.log.contains("no more input formats"));
  REQUIRE_FALSE(r.frames.isEmpty());

  // The device must adapt to what the producer actually publishes rather than
  // hold out for what was requested.
  CHECK(r.log.contains("PipeWire negotiated: 320 x 240"));

  int nonBlank = 0, withBars = 0;
  for(const auto& f : r.frames)
  {
    if(distinctColours(f) > 1)
      nonBlank++;
    if(hasSmpteBars(f))
      withBars++;
  }
  INFO(
      "grabbed " << r.frames.size() << " frames, " << nonBlank
                 << " non-blank, " << withBars << " carrying the bars");
  // Reported, not relied on: the shader's TEX_DIMENSIONS cell is painted from
  // the texture's size alone, so every frame is non-blank even when not one
  // pixel of video arrived. Measured -- with the input advertising only its
  // exact request, this run grabs 12 non-blank frames and zero with bars.
  CHECK(nonBlank > 0);
  // The assertion that cannot be satisfied by a black frame, a stale texture
  // or a "no signal" card: videotestsrc's seven saturated bars, in order,
  // sampled out of the passthrough shader's top-left cell.
  REQUIRE(withBars > 0);
}
