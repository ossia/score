// Pins that an offscreen grab waits for the execution to deliver the graph
// in wall-clock time, not for a frame count (TASKS N90). A Javascript process
// whose first execution tick takes 500 ms sits next to a solid-colour ISF
// shader cabled to the Window; renderFrames(120) on an empty render list
// finishes long before that tick, and the grab must still return the shader's
// colour.
//
// Drives the application binary through --script, the path lin-grab.sh uses.

#include <QFile>
#include <QImage>
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
  if(!QFile::exists(corpusDir() + "/isf-solid-color.fs"))
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
}

TEST_CASE("offscreen grab waits for a slow first execution tick (N90)", "[integration][gfx][n90]")
{
  if(!ready())
    SKIP("needs the score binary, the gfx corpus and a display");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString js = dir.filePath("slow-start.js");
  const QString png = dir.filePath("slow-start.png");

  const QString qml
      = "import Score 1.0\\n"
        "Script {\\n"
        "  ValueOutlet { id: out1 }\\n"
        "  property int slowStart: { var t0 = Date.now(); while(Date.now() - t0 < 500) {} return 0; }\\n"
        "  tick: function(token, state) { out1.value = 1; }\\n"
        "}\\n";

  QString src;
  src += "var OUT = \"" + png + "\";\n";
  src += "Score.createDevice(\"Window\", \"5a181207-7d40-4ad8-814e-879fcdf8cc31\", "
         "{\"Mode\": 1, \"Outputs\": [], \"InputWidth\": 64, \"InputHeight\": 64});\n";
  src += "var s = Score.find(\"Scenario.1\"); if (s) Score.remove(s);\n";
  src += "var itv = Score.rootInterval();\n";
  src += "var jsp = Score.createProcess(itv, \"Javascript\", \"" + qml + "\");\n";
  src += "if (!jsp) { console.log(\"SCENE-ERROR: no javascript process\"); Qt.exit(9); }\n";
  src += "var proc = Score.createProcess(itv, "
         "\"74ca45ff-92c9-44a0-8f1a-754dea05ee1b\", \""
         + corpusDir() + "/isf-solid-color.fs\");\n";
  src += "if (!proc) { console.log(\"SCENE-ERROR: no shader process\"); Qt.exit(9); }\n";
  src += "Score.setAddress(Score.outlet(proc, 0), \"Window:/\");\n";
  src += "var dev = Score.device(\"Window\");\n";
  src += "Score.play();\n";
  src += "dev.setStepRate(60);\n";
  src += "dev.renderFrames(120);\n";
  src += "dev.grabTo(OUT);\n";
  src += "Score.stop();\n";
  src += "console.log(\"G2-GRABBED\");\n";
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
  p.start(
      appBinary(),
      {"--no-gui", "--no-restore", "--script", js, "--wait", "0"});
  const bool finished = p.waitForStarted(30000) && p.waitForFinished(180000);
  if(!finished)
  {
    p.kill();
    p.waitForFinished(5000);
  }
  const QString log = QString::fromUtf8(p.readAll());
  INFO(log.toStdString());
  REQUIRE(finished);
  REQUIRE(log.contains("G2-GRABBED"));
  REQUIRE_FALSE(log.contains("SCENE-ERROR"));
  CHECK_FALSE(log.contains("nothing rendered into"));
  REQUIRE(QFile::exists(png));

  const QImage img = QImage{png}.convertToFormat(QImage::Format_RGB32);
  REQUIRE_FALSE(img.isNull());
  const QRgb c = img.pixel(img.width() / 2, img.height() / 2);
  INFO("center=(" << qRed(c) << "," << qGreen(c) << "," << qBlue(c) << ")");
  CHECK(qRed(c) > 200);
  CHECK(qGreen(c) < 50);
  CHECK(qBlue(c) > 200);
}
