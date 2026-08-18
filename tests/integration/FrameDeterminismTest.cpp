// The on-demand frame stepping every golden-image claim rests on:
// GfxContext::renderFrames, WindowDevice::renderFrames / grabFrame /
// setStepRate and the synthetic step clock (a16977f69a, e9ca948e21,
// 0b6f3a34bd).
//
// None of it had a registered ctest. The only harnesses that reached it,
// golden-render/frame-determinism.sh and golden-render/sweep.sh, were not
// add_test'ed and need an out-of-repo shader corpus; setStepRate had no caller
// anywhere in the tree. The registered gfx harnesses all use sleep-then-grabTo
// and never touch grabFrame.
//
// Separate processes rather than repeated grabs in one, because anything
// cached in the process would hide exactly the nondeterminism this looks for.
// The in-tree corpus shader draws TIME, TIMEDELTA, PROGRESS and FRAMEINDEX as
// bars, so it fails if any of them still follows a wall clock; a static shader
// would pass whether or not the step clock works.

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
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

struct Grab
{
  int exitCode{-1};
  bool crashed{true};
  QString log;
  QString png;
};

//! Run one score process that builds the scene, plays it, and grabs.
//! `body` is the JS between Score.play() and Qt.exit(0).
Grab render(const QTemporaryDir& dir, const QString& name, const QString& body)
{
  const QString js = dir.filePath(name + ".js");
  const QString png = dir.filePath(name + ".png");

  QString src;
  src += "var UUID_ISF    = \"74ca45ff-92c9-44a0-8f1a-754dea05ee1b\";\n";
  src += "var UUID_WINDOW = \"5a181207-7d40-4ad8-814e-879fcdf8cc31\";\n";
  src += "var OUT = \"" + png + "\";\n";
  src += "Score.createDevice(\"Window\", UUID_WINDOW, {});\n";
  src += "var s = Score.find(\"Scenario.1\"); if (s) Score.remove(s);\n";
  src += "var root = Score.rootInterval();\n";
  src += "var proc = Score.createProcess(root, UUID_ISF, \"" + corpusDir()
         + "/isf-time-uniforms.fs\");\n";
  src += "if (!proc) { console.log(\"SCENE-ERROR: no process\"); Qt.exit(9); }\n";
  src += "Score.setAddress(Score.outlet(proc, 0), \"Window:/\");\n";
  src += "var dev = Score.device(\"Window\");\n";
  src += "Score.play();\n";
  src += body + "\n";
  src += "Qt.exit(0);\n";

  {
    QFile f{js};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(src.toUtf8());
  }

  auto env = QProcessEnvironment::systemEnvironment();
  // Without this the Window device opens a real window and grabTo falls back
  // to QScreen::grabWindow, i.e. the desktop: a byte-stable picture that would
  // "pass" a determinism check while proving nothing.
  env.insert("SCORE_FORCE_OFFSCREEN_WINDOW", "Window");
  env.insert("SCORE_AUDIO_BACKEND", "dummy");
  env.insert("SCORE_DISABLE_AUDIOPLUGINS", "1");

  QProcess p;
  p.setProcessEnvironment(env);
  p.setProcessChannelMode(QProcess::MergedChannels);
  p.start(
      appBinary(), {"--no-gui", "--no-restore", "--script", js, "--wait", "0"});

  Grab g;
  g.png = png;
  if(!p.waitForStarted(30000) || !p.waitForFinished(180000))
  {
    p.kill();
    p.waitForFinished(5000);
    return g;
  }
  g.log = QString::fromUtf8(p.readAll());
  g.crashed = p.exitStatus() != QProcess::NormalExit;
  g.exitCode = p.exitCode();
  return g;
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

//! Compared as a digest rather than as a QByteArray: Catch2 stringifies a PNG
//! byte by byte, which buries the failure it is reporting.
std::string digest(const QString& path)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
    return "<unreadable>";
  const auto data = f.readAll();
  return QCryptographicHash::hash(data, QCryptographicHash::Sha256)
      .toHex()
      .left(16)
      .toStdString();
}

//! A grab is only evidence if it really came from a readback of a rendered
//! frame; both warnings below mean it did not.
void requireRealRender(const Grab& g)
{
  INFO(g.log.toStdString());
  REQUIRE_FALSE(g.crashed);
  REQUIRE(g.exitCode == 0);
  REQUIRE_FALSE(g.log.contains("capturing the SCREEN"));
  REQUIRE_FALSE(g.log.contains("nothing rendered into"));
  REQUIRE(QFile::exists(g.png));
  // Two blank frames match trivially: without this the easiest way to pass a
  // determinism test is to render nothing at all.
  REQUIRE(distinctColours(g.png) > 1);
}

bool ready()
{
  if(appBinary().isEmpty() || !QFile::exists(appBinary()))
    return false;
  if(corpusDir().isEmpty()
     || !QFile::exists(corpusDir() + "/isf-time-uniforms.fs"))
    return false;
  // The offscreen QPA has no GL: the readback comes back as a single flat
  // colour, so there is nothing to compare.
  return qEnvironmentVariableIsSet("DISPLAY")
         || qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
}
}

TEST_CASE("the same scene stepped the same way renders the same frame", "[integration][gfx][determinism]")
{
  if(!ready())
    SKIP("needs the score binary, the gfx corpus and a display");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  auto a = render(dir, "a", "dev.grabFrame(30, OUT);");
  requireRealRender(a);
  auto b = render(dir, "b", "dev.grabFrame(30, OUT);");
  requireRealRender(b);

  CHECK(digest(a.png) == digest(b.png));
}

TEST_CASE("the step clock follows the frame counter", "[integration][gfx][determinism]")
{
  if(!ready())
    SKIP("needs the score binary, the gfx corpus and a display");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  auto f30 = render(dir, "f30", "dev.grabFrame(30, OUT);");
  requireRealRender(f30);
  auto f60 = render(dir, "f60", "dev.grabFrame(60, OUT);");
  requireRealRender(f60);

  // TIME, TIMEDELTA, PROGRESS and FRAMEINDEX are all drawn: if the process
  // clock did not advance with the counter these would be the same picture.
  CHECK(digest(f30.png) != digest(f60.png));
}

TEST_CASE("renderFrames composes across calls", "[integration][gfx][determinism]")
{
  if(!ready())
    SKIP("needs the score binary, the gfx corpus and a display");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  auto ten = render(
      dir, "ten", "for(var i = 0; i < 10; i++) dev.renderFrames(1);\n"
                  "dev.grabFrame(0, OUT);");
  requireRealRender(ten);
  auto one = render(dir, "one", "dev.grabFrame(10, OUT);");
  requireRealRender(one);

  // The frame counter is kept across calls, so ten single steps land on the
  // same frame as one ten-frame step.
  CHECK(digest(ten.png) == digest(one.png));
}

TEST_CASE("renderFrames does nothing for a non-positive count", "[integration][gfx][determinism]")
{
  if(!ready())
    SKIP("needs the score binary, the gfx corpus and a display");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  auto plain = render(dir, "plain", "dev.grabFrame(30, OUT);");
  requireRealRender(plain);
  auto noop = render(
      dir, "noop", "dev.renderFrames(0);\ndev.renderFrames(-1);\n"
                   "dev.grabFrame(30, OUT);");
  requireRealRender(noop);

  CHECK(digest(plain.png) == digest(noop.png));
}

TEST_CASE("setStepRate changes the step the clock takes", "[integration][gfx][determinism]")
{
  if(!ready())
    SKIP("needs the score binary, the gfx corpus and a display");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  auto def = render(dir, "def", "dev.grabFrame(30, OUT);");
  requireRealRender(def);

  auto half = render(dir, "half", "dev.setStepRate(30.0);\ndev.grabFrame(30, OUT);");
  requireRealRender(half);
  // At 30 fps the same 30 frames span twice the time, so TIME and TIMEDELTA
  // both move. Nothing else in the tree calls setStepRate at all.
  CHECK(digest(def.png) != digest(half.png));

  auto same = render(dir, "same", "dev.setStepRate(60.0);\ndev.grabFrame(30, OUT);");
  requireRealRender(same);
  // 60 is the default, so asking for it explicitly must reproduce the default
  // run exactly -- otherwise the case above would pass on any perturbation.
  CHECK(digest(def.png) == digest(same.png));
}
