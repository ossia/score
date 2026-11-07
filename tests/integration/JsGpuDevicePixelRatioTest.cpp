// A GPU Javascript script lays its scene out in render-size units, drawn 1:1
// into its render-size texture, whatever the device pixel ratio of the screen
// the output window is on. Qt Quick takes the ratio from the render window it
// is given, and the script's render window is the output window. The ratio
// the script itself sees is 1 too, from its tick: a script choosing an @2x
// image or sizing a Canvas by it would otherwise lay out at twice its size.
//
// Drives the application binary through --script, with an on-screen Window
// device, at QT_SCALE_FACTOR 1 and 2.

#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <score_test/AppProcess.hpp>

#include <cmath>

namespace
{
namespace app = score::test::app;

// A red square at the centre of the scene, a blue one at a quarter of it, a
// white text block centred.
const char* const k_script = R"QML(import QtQuick
import Score
Script {
  property bool reported: false
  tick: function(token, state) {
    if(!reported && scene.Window.window) {
      reported = true;
      console.log("JS-GPU-TICK-DPR " + scene.Window.window.devicePixelRatio);
    }
  }
  TextureOutlet {
    objectName: "Output"
    item: Item {
      id: scene
      anchors.fill: parent
      Rectangle { anchors.fill: parent; color: "black" }
      Rectangle {
        width: 16; height: 16; color: "#ff0000"
        x: parent.width / 2 - 8; y: parent.height / 2 - 8
      }
      Rectangle {
        width: 8; height: 8; color: "#0000ff"
        x: parent.width / 4 - 4; y: parent.height / 4 - 4
      }
      Text {
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 3 / 4 - height / 2
        text: "HELLO"; color: "white"; font.pixelSize: 24
      }
    }
  }
}
)QML";

struct Centroid
{
  double x{-1}, y{-1};
  int count{};
};

template <typename F>
Centroid centroid(const QImage& img, F&& match)
{
  double sx{}, sy{};
  int n{};
  for(int y = 0; y < img.height(); ++y)
    for(int x = 0; x < img.width(); ++x)
      if(const QRgb c = img.pixel(x, y); match(qRed(c), qGreen(c), qBlue(c)))
      {
        sx += x + 0.5;
        sy += y + 0.5;
        ++n;
      }
  if(n == 0)
    return {};
  return {sx / n / img.width(), sy / n / img.height(), n};
}
}

TEST_CASE(
    "a GPU Javascript scene is placed the same at every device pixel ratio",
    "[integration][js][gpu][gui]")
{
#if !defined(SCORE_HAS_GPU_JS)
  SKIP("Javascript GPU execution is required");
#else
  if(!app::binary_available() || !app::has_display())
    SKIP("needs the score binary and a display");

  const QString scale = GENERATE(QStringLiteral("1"), QStringLiteral("2"));
  CAPTURE(scale.toStdString());

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString png = dir.filePath("js.png");
  const QString qml = app::write_file(dir, "probe.qml", k_script);

  QString src;
  src += "var OUT = " + app::js_string(png) + ";\n";
  src += "Score.createDevice(\"Window\", \"5a181207-7d40-4ad8-814e-879fcdf8cc31\", "
         "{\"Mode\": 0, \"Outputs\": [], \"InputWidth\": 320, \"InputHeight\": 180});\n";
  src += "var s = Score.find(\"Scenario.1\"); if (s) Score.remove(s);\n";
  src += "var proc = Score.createProcess(Score.rootInterval(), "
         "\"846a5de5-47f9-46c5-a898-013cb20951d0\", "
         + app::js_string(qml) + ");\n";
  src += "if (!proc) { console.log(\"SCENE-ERROR: no process\"); Qt.exit(9); }\n";
  src += "Score.setAddress(Score.outlet(proc, 0), \"Window:/\");\n";
  src += "var dev = Score.device(\"Window\");\n";
  src += "dev.setStepRate(60);\n";
  src += "Score.play();\n";
  src += "dev.renderFrames(30);\n";
  src += "dev.grabTo(OUT);\n";
  src += "console.log(\"JS-GPU-GRABBED\");\n";
  src += "Qt.exit(0);\n";

  app::options opts;
  opts.on_screen = true;
  opts.environment = {{"QT_SCALE_FACTOR", scale}, {"QT_SCREEN_SCALE_FACTORS", ""}};
  const auto res = app::run_script(app::write_file(dir, "js.js", src.toUtf8()), opts);
  INFO(res.output.toStdString());
  REQUIRE(res.finished);
  REQUIRE(res.output.contains("JS-GPU-GRABBED"));
  // The ratio the script's own code sees.
  CHECK(res.output.contains("JS-GPU-TICK-DPR 1\n"));

  REQUIRE(QFile::exists(png));
  const QImage img = QImage{png}.convertToFormat(QImage::Format_RGB32);
  REQUIRE_FALSE(img.isNull());
  CAPTURE(img.width(), img.height());

  // The window may show the texture at any size; positions are compared as
  // fractions of it.
  const auto red = centroid(img, [](int r, int g, int b) { return r > 160 && g < 80 && b < 80; });
  const auto blue = centroid(img, [](int r, int g, int b) { return b > 160 && r < 80 && g < 80; });
  const auto white
      = centroid(img, [](int r, int g, int b) { return r > 160 && g > 160 && b > 160; });
  CAPTURE(red.x, red.y, red.count, blue.x, blue.y, blue.count, white.x, white.y, white.count);

  REQUIRE(red.count > 0);
  REQUIRE(blue.count > 0);
  REQUIRE(white.count > 0);
  constexpr double tolerance = 0.03;
  CHECK(std::abs(red.x - 0.5) < tolerance);
  CHECK(std::abs(red.y - 0.5) < tolerance);
  CHECK(std::abs(blue.x - 0.25) < tolerance);
  CHECK(std::abs(blue.y - 0.25) < tolerance);
  CHECK(std::abs(white.x - 0.5) < tolerance);
  CHECK(std::abs(white.y - 0.75) < tolerance);
#endif
}
