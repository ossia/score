// Pins the stepped execution clock (TASKS N51, and the N90 follow-up).
//
// An Automation drives the float input of an ISF shader cabled to an
// offscreen Window; the shader writes the value with 16 bits of precision and
// FRAMEINDEX. With setStepRate(60) armed before Score.play(), renderFrames(N)
// runs the execution for exactly N/60 s, synchronously:
// - two separate processes grab byte-identical frames;
// - the value is the automation's at N/60 s (a linear 0 -> 1 ramp over the
//   root interval's default 15 s).
// A Javascript process whose first tick takes 500 ms does not change the
// frame count: the grab is frame N-1, with no settling frames added.
// Stopping and playing again starts the step count over.
//
// Drives the application binary through --script, the path lin-grab.sh uses.

#include <QFile>
#include <QImage>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>

#include <catch2/catch_approx.hpp>
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

QString corpusDir()
{
#if defined(GFX_TEST_CORPUS_DIR)
  return QStringLiteral(GFX_TEST_CORPUS_DIR);
#else
  return {};
#endif
}

bool ready()
{
  if(appBinary().isEmpty() || !QFile::exists(appBinary()))
    return false;
  if(!QFile::exists(corpusDir() + "/isf-i3n51-exec-probe.fs"))
    return false;
  if(qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen")
    return false;
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
  return qEnvironmentVariableIsSet("DISPLAY")
         || qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
#else
  return true;
#endif
}

constexpr double rootDurationSeconds = 15.;
constexpr double stepRate = 60.;

struct Grab
{
  bool ok{};
  QString log;
  QImage img;

  double value() const
  {
    const QRgb c = img.pixel(img.width() / 2, img.height() / 2);
    return (qRed(c) + qGreen(c) / 255.) / 255.;
  }
  int frameIndex() const
  {
    return qBlue(img.pixel(img.width() / 2, img.height() / 2));
  }
};

enum class Order
{
  StepThenPlay,
  PlayThenStep
};

QString grabBody(int frames, Order order)
{
  QString src;
  if(order == Order::StepThenPlay)
    src += "dev.setStepRate(60);\nScore.play();\n";
  else
    src += "Score.play();\ndev.setStepRate(60);\n";
  src += "dev.renderFrames(" + QString::number(frames) + ");\n";
  return src;
}

Grab run(const QTemporaryDir& dir, const QString& name, const QString& body, bool slowStart)
{
  const QString js = dir.filePath(name + ".js");
  const QString png = dir.filePath(name + ".png");

  QString src;
  src += "var OUT = \"" + png + "\";\n";
  src += "Score.createDevice(\"Window\", \"5a181207-7d40-4ad8-814e-879fcdf8cc31\", "
         "{\"Mode\": 1, \"Outputs\": [], \"InputWidth\": 64, \"InputHeight\": 64});\n";
  src += "var s = Score.find(\"Scenario.1\"); if (s) Score.remove(s);\n";
  src += "var itv = Score.rootInterval();\n";
  src += "var proc = Score.createProcess(itv, "
         "\"74ca45ff-92c9-44a0-8f1a-754dea05ee1b\", \""
         + corpusDir() + "/isf-i3n51-exec-probe.fs\");\n";
  src += "if (!proc) { console.log(\"SCENE-ERROR: no shader process\"); Qt.exit(9); }\n";
  src += "Score.setAddress(Score.outlet(proc, 0), \"Window:/\");\n";
  src += "if (!Score.automate(itv, Score.inlet(proc, 0))) { "
         "console.log(\"SCENE-ERROR: no automation\"); Qt.exit(9); }\n";
  if(slowStart)
  {
    const QString qml
        = "import Score 1.0\\n"
          "Script {\\n"
          "  ValueOutlet { id: out1 }\\n"
          "  property int slowStart: { var t0 = Date.now(); while(Date.now() - t0 < 500) {} return 0; }\\n"
          "  tick: function(token, state) { out1.value = 1; }\\n"
          "}\\n";
    src += "if (!Score.createProcess(itv, \"Javascript\", \"" + qml
           + "\")) { console.log(\"SCENE-ERROR: no javascript process\"); Qt.exit(9); }\n";
  }
  src += "var dev = Score.device(\"Window\");\n";
  src += body;
  src += "dev.grabTo(OUT);\n";
  src += "Score.stop();\n";
  src += "console.log(\"I3-GRABBED\");\n";
  src += "Qt.exit(0);\n";
  {
    QFile f{js};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(src.toUtf8());
  }

  auto env = QProcessEnvironment::systemEnvironment();
  env.insert("SCORE_FORCE_OFFSCREEN_WINDOW", "Window");
  env.insert("SCORE_AUDIO_BACKEND", "dummy");
  env.insert("SCORE_DISABLE_AUDIOPLUGINS", "1");
  env.insert("QT_FORCE_STDERR_LOGGING", "1");
  env.insert("QT_ASSUME_STDERR_HAS_CONSOLE", "1");

  QProcess p;
  p.setProcessEnvironment(env);
  p.setProcessChannelMode(QProcess::MergedChannels);
  p.start(appBinary(), {"--no-gui", "--no-restore", "--script", js, "--wait", "0"});

  Grab r;
  if(!p.waitForStarted(30000) || !p.waitForFinished(180000))
  {
    p.kill();
    p.waitForFinished(5000);
    return r;
  }
  r.log = QString::fromUtf8(p.readAll());
  // Judged on the script reaching its end: static teardown is not under test.
  r.ok = r.log.contains("I3-GRABBED") && !r.log.contains("SCENE-ERROR");
  if(QFile::exists(png))
    r.img = QImage{png}.convertToFormat(QImage::Format_RGB32);
  return r;
}

double expectedValue(int frames)
{
  return frames / stepRate / rootDurationSeconds;
}
}

TEST_CASE(
    "stepped execution: an automation-driven frame is reproducible",
    "[integration][gfx][determinism][n51]")
{
  if(!ready())
    SKIP("needs the score binary, the gfx corpus and a display");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  auto a = run(dir, "a", grabBody(120, Order::StepThenPlay), false);
  INFO(a.log.toStdString());
  REQUIRE(a.ok);
  REQUIRE_FALSE(a.img.isNull());

  auto b = run(dir, "b", grabBody(120, Order::StepThenPlay), false);
  INFO(b.log.toStdString());
  REQUIRE(b.ok);
  REQUIRE_FALSE(b.img.isNull());

  auto c = run(dir, "c", grabBody(30, Order::StepThenPlay), false);
  INFO(c.log.toStdString());
  REQUIRE(c.ok);
  REQUIRE_FALSE(c.img.isNull());

  INFO("value after 120 frames: " << a.value() << " then " << b.value());
  INFO("value after 30 frames: " << c.value());
  CHECK(a.img == b.img);
  CHECK(a.value() == Catch::Approx(expectedValue(120)).margin(3e-4));
  CHECK(c.value() == Catch::Approx(expectedValue(30)).margin(3e-4));
  CHECK(a.frameIndex() == 119);
  CHECK(c.frameIndex() == 29);
}

TEST_CASE(
    "stepped execution: a slow first tick does not change the grabbed frame",
    "[integration][gfx][n51][n90]")
{
  if(!ready())
    SKIP("needs the score binary, the gfx corpus and a display");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  SECTION("stepping armed before play")
  {
    auto r = run(dir, "slow-armed", grabBody(120, Order::StepThenPlay), true);
    INFO(r.log.toStdString());
    REQUIRE(r.ok);
    CHECK_FALSE(r.log.contains("nothing rendered into"));
    REQUIRE_FALSE(r.img.isNull());
    INFO("value: " << r.value() << " frame index: " << r.frameIndex());
    CHECK(r.frameIndex() == 119);
    CHECK(r.value() == Catch::Approx(expectedValue(120)).margin(3e-4));
  }

  SECTION("stepping taken over after play")
  {
    auto r = run(dir, "slow-takeover", grabBody(120, Order::PlayThenStep), true);
    INFO(r.log.toStdString());
    REQUIRE(r.ok);
    CHECK_FALSE(r.log.contains("nothing rendered into"));
    REQUIRE_FALSE(r.img.isNull());
    INFO("value: " << r.value() << " frame index: " << r.frameIndex());
    CHECK(r.frameIndex() == 119);
    CHECK(r.value() >= expectedValue(120) - 3e-4);
  }
}

TEST_CASE(
    "stepped execution: playing again starts the step count over",
    "[integration][gfx][n51]")
{
  if(!ready())
    SKIP("needs the score binary, the gfx corpus and a display");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  auto r = run(
      dir, "replay",
      grabBody(60, Order::StepThenPlay) + "Score.stop();\n"
          + grabBody(30, Order::StepThenPlay),
      false);
  INFO(r.log.toStdString());
  REQUIRE(r.ok);
  REQUIRE_FALSE(r.img.isNull());
  INFO("value: " << r.value() << " frame index: " << r.frameIndex());
  CHECK(r.value() == Catch::Approx(expectedValue(30)).margin(3e-4));
  CHECK(r.frameIndex() == 29);
}
