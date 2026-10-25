// A GPU Javascript script draws into the offscreen Window when the frames are
// rendered by a script with renderFrames(). The script's gfx node is
// registered from a call queued on the UI thread; renderFrames() blocks that
// thread, and the node only appeared after the last frame: its edge to the
// window was dropped and the grab was black.
//
// Drives the application binary through --script.

#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <score_test/AppProcess.hpp>

namespace
{
namespace app = score::test::app;

const char* const k_script = R"QML(import QtQuick
import Score
Script {
  TextureOutlet {
    objectName: "Output"
    item: Rectangle { anchors.fill: parent; color: "#00ff00" }
  }
}
)QML";

const char* const k_pass = R"ISF(/*{ "ISFVSN": "2", "INPUTS": [ { "NAME": "inputImage", "TYPE": "image" } ] }*/
void main() { gl_FragColor = IMG_THIS_PIXEL(inputImage); }
)ISF";
}

TEST_CASE(
    "a GPU Javascript script draws in frames rendered by renderFrames",
    "[integration][js][gpu][gui]")
{
#if !defined(SCORE_HAS_GPU_JS)
  SKIP("Javascript GPU execution is required");
#else
  if(!app::binary_available() || !app::has_display())
    SKIP("needs the score binary and a display");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString png = dir.filePath("js.png");
  const QString qml = app::write_file(dir, "green.qml", k_script);
  const QString pass = app::write_file(dir, "pass.fs", k_pass);

  QString src;
  src += "var OUT = " + app::js_string(png) + ";\n";
  src += "Score.createDevice(\"Window\", \"5a181207-7d40-4ad8-814e-879fcdf8cc31\", "
         "{\"Mode\": 1, \"Outputs\": [], \"InputWidth\": 64, \"InputHeight\": 64});\n";
  src += "var s = Score.find(\"Scenario.1\"); if (s) Score.remove(s);\n";
  src += "var proc = Score.createProcess(Score.rootInterval(), "
         "\"846a5de5-47f9-46c5-a898-013cb20951d0\", "
         + app::js_string(qml) + ");\n";
  src += "if (!proc) { console.log(\"SCENE-ERROR: no process\"); Qt.exit(9); }\n";
  // Through a shader: the script's texture reaches the window by a cable to
  // another gfx node, as in the corpus scores that rendered black.
  src += "var isf = Score.createProcess(Score.rootInterval(), "
         "\"74ca45ff-92c9-44a0-8f1a-754dea05ee1b\", "
         + app::js_string(pass) + ");\n";
  src += "if (!isf) { console.log(\"SCENE-ERROR: no shader\"); Qt.exit(9); }\n";
  src += "Score.createCable(Score.outlet(proc, 0), Score.inlet(isf, 0));\n";
  src += "Score.setAddress(Score.outlet(isf, 0), \"Window:/\");\n";
  src += "var dev = Score.device(\"Window\");\n";
  src += "dev.setStepRate(60);\n";
  src += "Score.play();\n";
  src += "dev.renderFrames(30);\n";
  src += "dev.grabTo(OUT);\n";
  src += "console.log(\"JS-GPU-GRABBED\");\n";
  src += "Qt.exit(0);\n";

  app::options opts;
  opts.offscreen_windows = QStringLiteral("Window");
  const auto res = app::run_script(app::write_file(dir, "js.js", src.toUtf8()), opts);
  INFO(res.output.toStdString());
  REQUIRE(res.finished);
  REQUIRE(res.output.contains("JS-GPU-GRABBED"));
  CHECK_FALSE(res.output.contains("still not registered"));

  REQUIRE(QFile::exists(png));
  const QImage img = QImage{png}.convertToFormat(QImage::Format_RGB32);
  REQUIRE_FALSE(img.isNull());
  const QRgb c = img.pixel(img.width() / 2, img.height() / 2);
  CAPTURE(qRed(c), qGreen(c), qBlue(c));
  CHECK(qRed(c) < 16);
  CHECK(qGreen(c) > 240);
  CHECK(qBlue(c) < 16);
#endif
}
