// Pins two properties of the offscreen Window device driven by
// renderFrames():
//
// - The step clock owns TIME and TIMEDELTA. With setStepRate(60) every stepped
//   frame advances TIME by exactly 1/60 and reports TIMEDELTA == 1/60, even
//   when execution ticks land between steps and write the transport's date
//   into the node, rather than TIMEDELTA = step date minus transport date
//   (seconds, or negative).
// - The grab is InputWidth x InputHeight, whatever `size` value was saved in
//   the device tree.
//
// Drives the application binary through --script.

#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <score_test/AppProcess.hpp>

namespace
{
namespace app = score::test::app;

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
  return app::binary_available() && app::has_display()
         && QFile::exists(corpusDir() + "/isf-step-clock-probe.fs");
}

struct Run
{
  bool ok{};
  QString log;
  QImage img;
};

//! The JS that builds the scene: a Window device with `settings` as its JSON
//! settings, fed by `shader`.
QString scene(const QString& settings, const QString& shader)
{
  QString src;
  src += "Score.createDevice(\"Window\", \"5a181207-7d40-4ad8-814e-879fcdf8cc31\", "
         + settings + ");\n";
  src += "var s = Score.find(\"Scenario.1\"); if (s) Score.remove(s);\n";
  src += "var proc = Score.createProcess(Score.rootInterval(), "
         "\"74ca45ff-92c9-44a0-8f1a-754dea05ee1b\", "
         + app::js_string(corpusDir() + "/" + shader) + ");\n";
  src += "if (!proc) { console.log(\"SCENE-ERROR: no process\"); Qt.exit(9); }\n";
  src += "Score.setAddress(Score.outlet(proc, 0), \"Window:/\");\n";
  return src;
}

//! One score process running `script`, after loading `score` when given.
//! `body` runs after Score.play() and may grab to OUT.
Run run(
    const QTemporaryDir& dir, const QString& name, const QString& script,
    const QString& body, const QString& score = {})
{
  const QString png = dir.filePath(name + ".png");

  QString src;
  src += "var OUT = " + app::js_string(png) + ";\n";
  src += script;
  src += "var dev = Score.device(\"Window\");\n";
  src += "Score.play();\n";
  src += body + "\n";
  src += "console.log(\"STEP-CLOCK-GRABBED\");\n";
  src += "Qt.exit(0);\n";

  app::options opts;
  if(!score.isEmpty())
    opts.documents << score;
  opts.offscreen_windows = QStringLiteral("Window");
  const auto res = app::run_script(app::write_file(dir, name + ".js", src.toUtf8()), opts);

  Run r;
  r.log = res.output;
  // Judged on the script reaching its end rather than on the exit status: what
  // happens at static teardown is not what this test is about.
  r.ok = res.finished && r.log.contains("STEP-CLOCK-GRABBED");
  if(QFile::exists(png))
    r.img = QImage{png}.convertToFormat(QImage::Format_RGB32);
  return r;
}
}

TEST_CASE("stepped TIMEDELTA is exactly one step on every frame", "[integration][gfx][determinism]")
{
  if(!ready())
    SKIP("needs the score binary, the gfx corpus and a display");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  // Several execution ticks land inside a 180-frame loop, so the transport
  // date is written between two steps.
  auto r = run(
      dir, "delta", scene("{}", "isf-step-clock-probe.fs"),
      "dev.setStepRate(60);\ndev.renderFrames(180);\ndev.grabTo(OUT);");
  INFO(r.log.toStdString());
  REQUIRE(r.ok);
  REQUIRE_FALSE(r.img.isNull());

  const int y = r.img.height() / 2;
  const QRgb counts = r.img.pixel(r.img.width() / 4, y);
  const QRgb frames = r.img.pixel(3 * r.img.width() / 4, y);

  INFO("frames with TIMEDELTA != 1/60: " << qRed(counts));
  INFO("frames whose TIME did not advance by 1/60: " << qGreen(counts));
  INFO("frames with TIMEDELTA < 0: " << qBlue(counts));
  INFO("frames counted: " << qRed(frames));
  REQUIRE(qRed(frames) >= 180);
  CHECK(qRed(counts) == 0);
  CHECK(qGreen(counts) == 0);
  CHECK(qBlue(counts) == 0);
}

namespace
{
//! Sets the value of the Window device's `size` address in a saved score, the
//! way a score saved while its window had another size carries it.
bool setSavedWindowSize(QJsonValue& v, double w, double h)
{
  bool found = false;
  if(v.isObject())
  {
    auto o = v.toObject();
    if(o.contains("Address"))
    {
      auto a = o["Address"].toObject();
      if(a["Name"].toString() == "size")
      {
        a["Value"] = QJsonObject{{"Vec2f", QJsonArray{w, h}}};
        o["Address"] = a;
        found = true;
      }
    }
    for(auto it = o.begin(); it != o.end(); ++it)
    {
      QJsonValue c = it.value();
      if(setSavedWindowSize(c, w, h))
      {
        it.value() = c;
        found = true;
      }
    }
    v = o;
  }
  else if(v.isArray())
  {
    auto a = v.toArray();
    for(int i = 0; i < a.size(); i++)
    {
      QJsonValue c = a[i];
      if(setSavedWindowSize(c, w, h))
      {
        a[i] = c;
        found = true;
      }
    }
    v = a;
  }
  return found;
}
}

TEST_CASE("the offscreen grab is InputWidth x InputHeight", "[integration][gfx]")
{
  if(!ready())
    SKIP("needs the score binary, the gfx corpus and a display");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  const QString settings
      = "{\"Mode\": 1, \"Outputs\": [], \"InputWidth\": 640, \"InputHeight\": 360}";

  SECTION("as created")
  {
    auto r = run(
        dir, "created", scene(settings, "isf-step-clock-probe.fs"),
        "dev.renderFrames(5);\ndev.grabTo(OUT);");
    INFO(r.log.toStdString());
    REQUIRE(r.ok);
    REQUIRE_FALSE(r.img.isNull());
    CHECK(r.img.width() == 640);
    CHECK(r.img.height() == 360);
  }

  SECTION("as loaded, with another size saved in the device tree")
  {
    const QString saved = dir.filePath("saved.score");
    auto b = run(
        dir, "build",
        scene(settings, "isf-step-clock-probe.fs") + "Score.saveAs("
            + app::js_string(saved) + ");\n",
        "");
    INFO(b.log.toStdString());
    REQUIRE(b.ok);
    REQUIRE(QFile::exists(saved));

    {
      QFile f{saved};
      REQUIRE(f.open(QIODevice::ReadOnly));
      QJsonValue doc = QJsonDocument::fromJson(f.readAll()).object();
      f.close();
      REQUIRE(setSavedWindowSize(doc, 1261., 962.));
      REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
      f.write(QJsonDocument{doc.toObject()}.toJson());
    }

    auto r = run(dir, "loaded", "", "dev.renderFrames(5);\ndev.grabTo(OUT);", saved);
    INFO(r.log.toStdString());
    REQUIRE(r.ok);
    REQUIRE_FALSE(r.img.isNull());
    CHECK(r.img.width() == 640);
    CHECK(r.img.height() == 360);
  }
}
