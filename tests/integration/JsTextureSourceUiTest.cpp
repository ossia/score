// Texture sources in a custom UI: created, destroyed, hidden and shown again
// while the document plays.
//
// `score --ui foo.qml` replaces the editor with a QML interface, and that
// interface shows what a process renders through Score.UI's TextureSource.
// Each one puts a preview node into the document's GfxContext and wires the
// process's texture outlet to it; each one taken away has to undo exactly that
// and nothing else. Both halves run while the graph is live, so a mistake
// there is not a wrong picture, it is the whole render going down.
//
// Two properties, both read off SCORE_GFX_TRACE:
//
//   * every texture source that appears connects -- one GFX-ADDEDGE per
//     source, to a sink nobody else is using;
//   * and costs ONE render list. A graph rebuild would rebuild the render list
//     of everything already running, so N sources appearing one at a time
//     would print O(N^2) creations instead of N, and the window that was
//     already rendering would blink on each one.
//
// Needs a real display: the offscreen platform never runs the Qt Quick scene
// graph, so a TextureSource is constructed and its renderer never is. The case
// says so and skips rather than passing on an empty log.

#include <QByteArray>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
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

//! The ISF filter process is named so a TextureSource can find it: the item
//! resolves its `process` property against every process's label and name.
const char* kProcessName = "tex";

struct Run
{
  int exitCode{-1};
  bool crashed{true};
  QString log;
};

QString write(const QTemporaryDir& dir, const QString& name, const QString& body)
{
  const QString path = dir.filePath(name);
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(body.toUtf8());
  f.close();
  return path;
}

//! Builds the document the UI then looks at: one self-contained ISF generator
//! on the root interval, its texture outlet addressed to an offscreen window
//! device, playing. It deliberately does NOT exit -- the UI drives the run and
//! calls Qt.exit() when it is done.
QString sceneScript()
{
  QString src;
  src += "var UUID_ISF = \"74ca45ff-92c9-44a0-8f1a-754dea05ee1b\";\n";
  src += "var UUID_WINDOW = \"5a181207-7d40-4ad8-814e-879fcdf8cc31\";\n";
  src += "Score.createDevice(\"Window\", UUID_WINDOW, {});\n";
  src += "var s = Score.find(\"Scenario.1\"); if (s) Score.remove(s);\n";
  src += "var root = Score.rootInterval();\n";
  src += "var flt = Score.createProcess(root, UUID_ISF, \"" + corpusDir()
         + "/isf-gradient-x.fs\");\n";
  src += "if (!flt) { console.log(\"SCENE-ERROR: no filter\"); Qt.exit(9); }\n";
  src += QStringLiteral("Score.setName(flt, \"%1\");\n").arg(kProcessName);
  src += "Score.setAddress(Score.outlet(flt, 0), \"Window:/\");\n";
  src += "Score.play();\n";
  src += "console.log(\"SCENE-OK\");\n";
  return src;
}

//! The part every UI in this file shares: the component, the two helpers and
//! the step timer. `body` is the switch over `k`, the step within a cycle.
QString uiFile(const QString& body)
{
  QString qml;
  qml += "import QtQuick\n";
  qml += "import Score.UI\n";
  qml += "Item {\n";
  qml += "  id: root\n";
  qml += "  width: 400; height: 200\n";
  qml += "  property var live: []\n";
  qml += "  property int step: 0\n";
  qml += "  property int cycle: 0\n";
  qml += "  Component { id: texComp; TextureSource {\n";
  qml += "    width: 64; height: 48\n";
  qml += QStringLiteral("    process: \"%1\"; port: 0\n").arg(kProcessName);
  qml += "  } }\n";
  qml += "  function spawn(n) {\n";
  qml += "    for(var i = 0; i < n; i++)\n";
  qml += "      root.live.push(texComp.createObject(root, {x: 4 + i*70, y: 4}))\n";
  qml += "    console.log(\"UI-SPAWNED \" + n)\n";
  qml += "  }\n";
  qml += "  function killAll() {\n";
  qml += "    for(var i = 0; i < root.live.length; i++) root.live[i].destroy()\n";
  qml += "    root.live = []\n";
  qml += "    console.log(\"UI-KILLED\")\n";
  qml += "  }\n";
  qml += "  function setVisible(v) {\n";
  qml += "    for(var i = 0; i < root.live.length; i++) root.live[i].visible = v\n";
  qml += "    console.log(v ? \"UI-SHOWN\" : \"UI-HIDDEN\")\n";
  qml += "  }\n";
  qml += "  Timer {\n";
  qml += "    interval: 400; running: true; repeat: true\n";
  qml += "    onTriggered: {\n";
  // The first steps are the document's: the script has to have built the
  // scene and started playing before a TextureSource can resolve anything.
  qml += "      root.step++\n";
  qml += "      if(root.step < 4) return\n";
  qml += "      var k = (root.step - 4) % 3\n";
  qml += body;
  qml += "    }\n";
  qml += "  }\n";
  qml += "}\n";
  return qml;
}

Run runUi(const QString& qml, const QString& js)
{
  auto env = QProcessEnvironment::systemEnvironment();
  // The window output must not map a real window: this is about the graph, and
  // a mapped window on a shared desktop is a nuisance either way.
  env.insert("SCORE_FORCE_OFFSCREEN_WINDOW", "Window");
  env.insert("SCORE_AUDIO_BACKEND", "dummy");
  env.insert("SCORE_DISABLE_AUDIOPLUGINS", "1");
  env.insert("SCORE_GFX_TRACE", "1");
  // Every verdict is read out of the child's log, and a process with no
  // console can otherwise route qDebug somewhere this pipe never sees.
  env.insert("QT_FORCE_STDERR_LOGGING", "1");
  env.insert("QT_ASSUME_STDERR_HAS_CONSOLE", "1");

  QProcess p;
  p.setProcessEnvironment(env);
  p.setProcessChannelMode(QProcess::MergedChannels);
  p.start(
      appBinary(),
      {"--no-gui", "--no-restore", "--wait", "0", "--ui", qml, "--script", js});

  Run r;
  if(!p.waitForStarted(30000) || !p.waitForFinished(180000))
  {
    p.kill();
    p.waitForFinished(5000);
    r.log = QString::fromUtf8(p.readAll());
    return r;
  }
  r.log = QString::fromUtf8(p.readAll());
  r.crashed = p.exitStatus() != QProcess::NormalExit;
  r.exitCode = p.exitCode();
  return r;
}

//! Byte offsets of every occurrence of `needle`.
std::vector<qsizetype> offsetsOf(const QString& log, const QString& needle)
{
  std::vector<qsizetype> r;
  for(auto pos = log.indexOf(needle); pos >= 0; pos = log.indexOf(needle, pos + 1))
    r.push_back(pos);
  return r;
}

//! How many `needle` fall in [from, to).
int countBetween(
    const QString& log, const QString& needle, qsizetype from, qsizetype to)
{
  int n = 0;
  for(auto off : offsetsOf(log, needle))
    if(off >= from && off < to)
      n++;
  return n;
}

constexpr auto kRenderList = "GFX-RENDERLIST created";
constexpr auto kSpawned = "UI-SPAWNED";
constexpr auto kKilled = "UI-KILLED";

void requireDisplay()
{
  REQUIRE_FALSE(appBinary().isEmpty());
  REQUIRE(QFile::exists(appBinary()));
  REQUIRE(QFile::exists(corpusDir() + "/isf-gradient-x.fs"));
}

//! A run whose graph never came up at all is an environment verdict, not a
//! failure: say which and skip. A graph that came up and then failed to
//! connect a preview is a real failure and must NOT land here.
bool graphCameUp(const Run& r)
{
  return r.log.contains(kRenderList);
}
}

TEST_CASE(
    "a custom UI adds and removes texture sources while playing",
    "[integration][gfx][js][ui]")
{
  requireDisplay();

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  // Three cycles of: spawn two, wait, destroy both.
  QString body;
  body += "      if(k === 0) {\n";
  body += "        root.cycle++\n";
  body += "        if(root.cycle > 3) { console.log(\"UI-DONE\"); Qt.exit(0); return }\n";
  body += "        root.spawn(2)\n";
  body += "      }\n";
  body += "      else if(k === 2) root.killAll()\n";

  const QString qml = write(dir, "sources.qml", uiFile(body));
  const QString js = write(dir, "scene.js", sceneScript());
  const auto r = runUi(qml, js);

  INFO(r.log.toStdString());
  CHECK_FALSE(r.crashed);
  CHECK(r.exitCode == 0);
  REQUIRE(r.log.contains("SCENE-OK"));

  if(!graphCameUp(r))
    SKIP("the render graph never came up: this run had no usable GPU or display");

  REQUIRE(r.log.contains("UI-DONE"));

  const auto spawns = offsetsOf(r.log, kSpawned);
  const auto kills = offsetsOf(r.log, kKilled);
  REQUIRE(spawns.size() == 3);
  REQUIRE(kills.size() == 3);

  // The scene graph has to have run: without it a TextureSource is built and
  // its renderer, which is what registers the preview, never is.
  if(countBetween(r.log, "GFX-ADDEDGE ok", spawns[0], kills[0]) == 0)
    SKIP("no preview connected: the Qt Quick scene graph never rendered");

  for(std::size_t i = 0; i < spawns.size(); i++)
  {
    INFO("cycle " << i);

    // Two sources appeared, so two edges into the graph...
    CHECK(countBetween(r.log, "GFX-ADDEDGE ok", spawns[i], kills[i]) == 2);

    // ...and two render lists, one each. This is the whole point: a rebuild
    // would recreate the render list of the window that is already rendering,
    // and of every preview already up, so this number would climb with the
    // cycle instead of staying at two.
    CHECK(countBetween(r.log, kRenderList, spawns[i], kills[i]) == 2);
  }
}

// The pin for a crash on the way out: the three connections TextureSource
// makes in its constructor are all from itself to itself, and ~QQuickItem
// emits windowChanged(nullptr) AFTER ~TextureSource has run. Qt then called
// handleWindowChanged on a half-destroyed object and aborted with "Called
// object is not of the correct type (class destructor may have already run)".
// Destroying a TextureSource from QML, which is how a custom UI removes one,
// did that every single time.
TEST_CASE(
    "destroying a texture source does not abort the application",
    "[integration][gfx][js][ui]")
{
  requireDisplay();

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  QString body;
  body += "      if(k === 0) {\n";
  body += "        root.cycle++\n";
  body += "        if(root.cycle > 1) { console.log(\"UI-DONE\"); Qt.exit(0); return }\n";
  body += "        root.spawn(1)\n";
  body += "      }\n";
  body += "      else if(k === 2) root.killAll()\n";

  const QString qml = write(dir, "one.qml", uiFile(body));
  const QString js = write(dir, "scene.js", sceneScript());
  const auto r = runUi(qml, js);

  INFO(r.log.toStdString());
  REQUIRE(r.log.contains(kKilled));
  CHECK_FALSE(r.log.contains("class destructor may have already run"));
  CHECK_FALSE(r.crashed);
  CHECK(r.exitCode == 0);
  CHECK(r.log.contains("UI-DONE"));
}

TEST_CASE(
    "hiding and showing a texture source leaves the graph alone",
    "[integration][gfx][js][ui]")
{
  requireDisplay();

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  // spawn two, hide, show, hide, show, destroy.
  QString body;
  body += "      if(root.step === 4) root.spawn(2)\n";
  body += "      else if(root.step === 7) root.setVisible(false)\n";
  body += "      else if(root.step === 10) root.setVisible(true)\n";
  body += "      else if(root.step === 13) root.setVisible(false)\n";
  body += "      else if(root.step === 16) root.setVisible(true)\n";
  body += "      else if(root.step === 19) root.killAll()\n";
  body += "      else if(root.step === 21) { console.log(\"UI-DONE\"); Qt.exit(0) }\n";

  const QString qml = write(dir, "visible.qml", uiFile(body));
  const QString js = write(dir, "scene.js", sceneScript());
  const auto r = runUi(qml, js);

  INFO(r.log.toStdString());
  CHECK_FALSE(r.crashed);
  CHECK(r.exitCode == 0);
  REQUIRE(r.log.contains("SCENE-OK"));

  if(!graphCameUp(r))
    SKIP("the render graph never came up: this run had no usable GPU or display");

  REQUIRE(r.log.contains("UI-DONE"));

  const auto spawns = offsetsOf(r.log, kSpawned);
  const auto kills = offsetsOf(r.log, kKilled);
  REQUIRE(spawns.size() == 1);
  REQUIRE(kills.size() == 1);

  if(countBetween(r.log, "GFX-ADDEDGE ok", spawns[0], kills[0]) == 0)
    SKIP("no preview connected: the Qt Quick scene graph never rendered");

  // Four visibility flips, and not one render list built between the last
  // source appearing and them all going away: hiding an item must not take
  // its preview out of the graph and put it back.
  CHECK(r.log.count("UI-HIDDEN") == 2);
  CHECK(r.log.count("UI-SHOWN") == 2);
  CHECK(countBetween(r.log, kRenderList, spawns[0], kills[0]) == 2);
}
