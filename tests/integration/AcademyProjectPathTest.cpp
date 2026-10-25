// An Academy Texture Loader whose path is stored relative to the project
// ("<PROJECT>:green.png", what saving a document next to its assets leaves)
// loads the image. Its Path was a plain line edit, which score hands to a GPU
// node as is: the loader opened "<PROJECT>:green.png" and showed nothing.
//
// Drives the application binary through --script: one run builds and saves
// the document next to the image, a second loads it and grabs a frame.

#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <score_test/AppProcess.hpp>

namespace
{
namespace app = score::test::app;

struct Run
{
  bool ok{};
  QString log;
};

Run run(const QTemporaryDir& dir, const QString& name, const QString& body,
        const QString& score = {})
{
  QString src = body + "console.log(\"ACADEMY-PATH-DONE\");\nQt.exit(0);\n";
  app::options opts;
  if(!score.isEmpty())
    opts.documents << score;
  opts.offscreen_windows = QStringLiteral("Window");
  const auto res = app::run_script(app::write_file(dir, name + ".js", src.toUtf8()), opts);
  return {res.finished && res.output.contains("ACADEMY-PATH-DONE"), res.output};
}
}

TEST_CASE(
    "an Academy Texture Loader opens a project-relative path",
    "[integration][academy][gfx]")
{
  if(!app::binary_available() || !app::has_display())
    SKIP("needs the score binary and a display");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  {
    QImage green{32, 32, QImage::Format_RGB32};
    green.fill(QColor{0, 255, 0});
    REQUIRE(green.save(dir.filePath("green.png")));
  }
  // The absolute path is the control: it loaded before the fix too.
  QString path = QStringLiteral("<PROJECT>:green.png");
  SECTION("project-relative") { }
  SECTION("absolute") { path = dir.filePath("green.png"); }
  const QString saved = dir.filePath("saved.score");
  const QString png = dir.filePath("grab.png");

  QString build;
  build += "Score.createDevice(\"Window\", \"5a181207-7d40-4ad8-814e-879fcdf8cc31\", "
           "{\"Mode\": 1, \"Outputs\": [], \"InputWidth\": 64, \"InputHeight\": 64});\n";
  build += "var s = Score.find(\"Scenario.1\"); if (s) Score.remove(s);\n";
  build += "var proc = Score.createProcess(Score.rootInterval(), "
           "\"5b2b3f9f-5e4d-4c2c-9c5d-4d2e6c1f3a8f\", \"\");\n";
  build += "if (!proc) { console.log(\"SCENE-ERROR: no loader\"); Qt.exit(9); }\n";
  build += "Score.setValue(Score.inlet(proc, \"Path\"), " + app::js_string(path) + ");\n";
  build += "Score.setAddress(Score.outlet(proc, 0), \"Window:/\");\n";
  build += "Score.saveAs(" + app::js_string(saved) + ");\n";
  auto b = run(dir, "build", build);
  INFO(b.log.toStdString());
  REQUIRE(b.ok);
  REQUIRE(QFile::exists(saved));

  QString grab;
  grab += "var dev = Score.device(\"Window\");\n";
  grab += "dev.setStepRate(60);\n";
  grab += "Score.play();\n";
  grab += "dev.renderFrames(10);\n";
  grab += "dev.grabTo(" + app::js_string(png) + ");\n";
  auto r = run(dir, "grab", grab, saved);
  INFO(r.log.toStdString());
  REQUIRE(r.ok);
  REQUIRE(QFile::exists(png));

  const QImage img = QImage{png}.convertToFormat(QImage::Format_RGB32);
  REQUIRE_FALSE(img.isNull());
  const QRgb c = img.pixel(img.width() / 2, img.height() / 2);
  CAPTURE(qRed(c), qGreen(c), qBlue(c));
  CHECK(qRed(c) < 16);
  CHECK(qGreen(c) > 240);
  CHECK(qBlue(c) < 16);
}
