// An ossia output used as a texture source, the way a custom QML interface
// uses it: the QML item is the only sink of the render graph, and everything
// the user does to the window -- resize it, drag it to a 2x screen, hide the
// panel, switch the source underneath -- lands on it.
//
// The scenarios covered:
//
//   * A whole multi-filter chain rendered through ONE
//     `UI.TextureSource { width: 4096; height: 4096; process: "rotate_zoom";
//     port: 0; visible: false }` and sampled by another item: hidden, and
//     pinned to a fixed size so that the physical buffer is not
//     device-pixel-ratio scaled -- score reads the item's *physical*
//     colour-buffer size verbatim, so on a 2x screen an unpinned 4096x4096
//     source would render the chain at 8192x8192.
//   * The opposite arrangement: a source laid out with `anchors.fill` inside a
//     Layout-sized Rectangle, so the item resizes continuously and at
//     fractional widths (a 350-high preview of 1280/720 content is 622.22
//     wide).
//   * A fixed 1280x720 source behind a `ShaderEffectSource { hideSource: true }`
//     gated with `active`, so that only the visible preview renders.
//   * The process under a live source destroyed and rebuilt, which is what
//     switching the source's backend or input mode does.
//
// What is NOT here: create/destroy/show/hide of the item itself, and the
// render-list accounting that goes with it. JsTextureSourceUiTest.cpp covers
// that from the real application's `--ui`, and TexturePreviewTest.cpp covers
// endpoint selection and stopped-node reuse. This file is about what the
// *rendered pixels* do when the item's geometry, its device pixel ratio, its
// visibility, its count, or the process under it changes.
//
// Everything here needs a real GPU: a TextureSource is a QQuickRhiItem, and
// without QRhi it is constructed and its renderer never is. Ask for an OpenGL
// context and skip, saying so, when the display cannot give one.

#include <Process/ProcessList.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <Gfx/TexturePort.hpp>
#include <JS/JSProcessModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <QColor>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QImage>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTemporaryDir>
#include <QThread>

#include <Transport/TransportInterface.hpp>
#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <cmath>
#include <cstdio>

#include <memory>

namespace
{
struct EnvironmentGuard
{
  const char* name;
  bool existed;
  QByteArray previous;

  EnvironmentGuard(const char* key, const QByteArray& value)
      : name{key}
      , existed{qEnvironmentVariableIsSet(key)}
      , previous{qgetenv(key)}
  {
    qputenv(name, value);
  }

  ~EnvironmentGuard()
  {
    if(existed)
      qputenv(name, previous);
    else
      qunsetenv(name);
  }
};

void spin(int ms)
{
  QElapsedTimer timer;
  timer.start();
  do
  {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QThread::msleep(2);
  } while(timer.elapsed() < ms);
}

template <typename Predicate>
bool eventually(Predicate&& predicate, int timeout_ms = 10000)
{
  QElapsedTimer timer;
  timer.start();
  do
  {
    spin(10);
    if(predicate())
      return true;
  } while(timer.elapsed() < timeout_ms);
  return false;
}

//! The label every TextureSource in this file resolves: TextureSource matches
//! a process on its metadata label or name, which is how a UI addresses a
//! named source such as "rotate_zoom" or "camera preview".
constexpr auto kSource = "test-tex-source";

// The document: one Javascript process whose texture outlet paints four known
// quadrants. Four, not two, so that a render which covers only part of the
// output is visible whichever axis it is short on.
//
// The root item follows its parent and the quadrants follow the root: score
// renders a texture outlet's item into a window sized to the graph's render
// size and resizes only that window and its contentItem, so a fixed-size item
// would be cropped rather than scaled and would say nothing about coverage.
//
// It also echoes whatever the UI sends it straight back, which is how the
// two-source case counts how many times ONE process answers: a preview is a
// render of the process, not another copy of it.

QString executionScript(
    const QString& tl, const QString& tr, const QString& bl, const QString& br)
{
  return QString::fromLatin1(R"QML(import QtQuick
import Score
Script {
  TextureOutlet {
    objectName: "Output"
    item: Item {
      width: parent ? parent.width : 480
      height: parent ? parent.height : 270
      Rectangle {
        anchors.left: parent.left; anchors.top: parent.top
        width: parent.width / 2; height: parent.height / 2
        color: "%1"
      }
      Rectangle {
        anchors.right: parent.right; anchors.top: parent.top
        width: parent.width / 2; height: parent.height / 2
        color: "%2"
      }
      Rectangle {
        anchors.left: parent.left; anchors.bottom: parent.bottom
        width: parent.width / 2; height: parent.height / 2
        color: "%3"
      }
      Rectangle {
        anchors.right: parent.right; anchors.bottom: parent.bottom
        width: parent.width / 2; height: parent.height / 2
        color: "%4"
      }
    }
  }
  uiEvent: function(v) { uiSend(v) }
  tick: function(token, state) { }
}
)QML")
      .arg(tl, tr, bl, br);
}

//! The process's own UI, which counts the answers the script sends back. The
//! counter has to live in QML, inside the plugin: this binary links a second
//! copy of JS::ProcessModel's metaobject, so a pointer-to-member connect to its
//! executionToUi from here connects to nothing at all. Emitting uiToExecution
//! from here is fine -- that resolves by signal index.
constexpr auto uiScript = R"QML(import QtQuick
import Score
ScriptUI {
  id: root
  width: 120
  height: 40
  property int echoes: 0
  executionEvent: function(v) { root.echoes = root.echoes + 1 }
}
)QML";

struct Pattern
{
  QColor tl, tr, bl, br;
  const char* tlName;
  const char* trName;
  const char* blName;
  const char* brName;
};

const Pattern rgby{Qt::red,   Qt::green, Qt::blue,  Qt::yellow,
                   "#ff0000", "#00ff00", "#0000ff", "#ffff00"};
const Pattern cmwg{Qt::cyan,  Qt::magenta, Qt::white, QColor{128, 128, 128},
                   "#00ffff", "#ff00ff",   "#ffffff", "#808080"};

JS::ProcessModel* addSource(
    const score::GUIApplicationContext& ctx, score::Document& doc,
    Scenario::IntervalModel& interval, const QString& label, const Pattern& p)
{
  const auto key = UuidKey<Process::ProcessModel>::fromString(
      QStringLiteral("846a5de5-47f9-46c5-a898-013cb20951d0"));
  auto* factory = ctx.interfaces<Process::ProcessFactoryList>().get(key);
  REQUIRE(factory != nullptr);

  std::vector<Id<Process::ProcessModel>> before;
  for(auto& candidate : interval.processes)
    before.push_back(candidate.id());

  CommandDispatcher<> dispatcher{doc.context().commandStack};
  dispatcher.submit<Scenario::Command::AddOnlyProcessToInterval>(
      interval, factory->concreteKey(), factory->customConstructionData(), QPointF{});

  JS::ProcessModel* process{};
  for(auto& candidate : interval.processes)
  {
    if(std::find(before.begin(), before.end(), candidate.id()) != before.end())
      continue;
    process = qobject_cast<JS::ProcessModel*>(&candidate);
  }
  REQUIRE(process != nullptr);

  process->metadata().setLabel(label);
  const auto result = process->setProgram(JS::QmlSource{
      executionScript(p.tlName, p.trName, p.blName, p.brName),
      QString::fromLatin1(uiScript)});
  REQUIRE(result.valid);
  process->programChanged();
  process->inletsChanged();
  process->outletsChanged();
  REQUIRE(process->isGpu());

  // `port: 0` addresses the texture outlet, the way a UI declares it.
  REQUIRE(process->outlets().size() == 1);
  REQUIRE(qobject_cast<Gfx::TextureOutlet*>(process->outlets()[0]) != nullptr);
  return process;
}

Gfx::TextureOutlet& textureOutlet(JS::ProcessModel& process)
{
  REQUIRE(process.outlets().size() == 1);
  auto* output = qobject_cast<Gfx::TextureOutlet*>(process.outlets()[0]);
  REQUIRE(output != nullptr);
  return *output;
}

// The UI: built in its own QQmlEngine so that it outlives the process it
// points at, which is the whole point of the destroy-and-recreate scenario.

QQuickItem* buildUi(QQmlEngine& engine, QQuickWindow& window, const QString& qml)
{
  auto* component = new QQmlComponent{&engine, &window};
  component->setData(qml.toUtf8(), QUrl{"file:///texture-source.qml"});
  INFO(component->errorString().toStdString());
  REQUIRE_FALSE(component->isError());
  auto* item = qobject_cast<QQuickItem*>(component->create());
  REQUIRE(item != nullptr);
  item->setParentItem(window.contentItem());
  return item;
}

// Readback.

bool near(const QColor& actual, const QColor& expected)
{
  constexpr int tolerance = 28;
  return std::abs(actual.red() - expected.red()) <= tolerance
         && std::abs(actual.green() - expected.green()) <= tolerance
         && std::abs(actual.blue() - expected.blue()) <= tolerance;
}

//! The four quadrant centres of `rect` (the window's logical coordinates),
//! read out of a grab whose pixels are `scale` per logical unit.
struct Quadrants
{
  QColor tl, tr, bl, br;

  std::string describe() const
  {
    return "tl=" + tl.name().toStdString() + " tr=" + tr.name().toStdString()
           + " bl=" + bl.name().toStdString() + " br=" + br.name().toStdString();
  }
};

Quadrants quadrantsOf(const QImage& image, const QRectF& rect, double scale)
{
  const auto at = [&](double fx, double fy) {
    const int x = int((rect.x() + rect.width() * fx) * scale);
    const int y = int((rect.y() + rect.height() * fy) * scale);
    if(x < 0 || y < 0 || x >= image.width() || y >= image.height())
      return QColor{};
    return image.pixelColor(x, y);
  };
  return {at(0.25, 0.25), at(0.75, 0.25), at(0.25, 0.75), at(0.75, 0.75)};
}

//! True when the source's four quadrants cover the whole of `rect`. Score's
//! blit is free to mirror vertically (PreviewRendererInvertY exists because
//! some backends are Y-up), so both orientations are accepted: this asserts
//! coverage and scale, not handedness.
bool hasPattern(const QImage& image, const QRectF& rect, double scale, const Pattern& p)
{
  if(image.isNull())
    return false;
  const auto q = quadrantsOf(image, rect, scale);
  const bool upright
      = near(q.tl, p.tl) && near(q.tr, p.tr) && near(q.bl, p.bl) && near(q.br, p.br);
  const bool flipped
      = near(q.tl, p.bl) && near(q.tr, p.br) && near(q.bl, p.tl) && near(q.br, p.tr);
  return upright || flipped;
}

bool isBlank(const QImage& image, const QRectF& rect, double scale, const QColor& bg)
{
  if(image.isNull())
    return false;
  const auto q = quadrantsOf(image, rect, scale);
  return near(q.tl, bg) && near(q.tr, bg) && near(q.bl, bg) && near(q.br, bg);
}

double grabScale(const QQuickWindow& window, const QImage& image)
{
  return window.width() > 0 ? double(image.width()) / window.width() : 1.;
}

//! The rectangle a QQuickItem occupies in its window, in logical units.
QRectF windowRect(QQuickItem& item)
{
  const auto topLeft = item.mapToScene(QPointF{0, 0});
  return QRectF{topLeft, QSizeF{item.width(), item.height()}};
}

std::string report(const QQuickWindow& window, const QImage& image, const QRectF& rect)
{
  std::string s = "item " + std::to_string(int(rect.width())) + "x"
                  + std::to_string(int(rect.height())) + " at "
                  + std::to_string(int(rect.x())) + "," + std::to_string(int(rect.y()))
                  + " in a " + std::to_string(image.width()) + "x"
                  + std::to_string(image.height()) + " grab";
  if(!image.isNull())
    s += "; read " + quadrantsOf(image, rect, grabScale(window, image)).describe();

  // A grab that does not say what it should is much easier to read as a
  // picture: SCORE_TEX_DUMP=/some/dir keeps every one this file looks at.
  static int n = 0;
  const auto dir = qgetenv("SCORE_TEX_DUMP");
  if(!dir.isEmpty() && !image.isNull())
  {
    const QString path
        = QString::fromUtf8(dir) + QStringLiteral("/grab-%1.png").arg(n++);
    image.save(path);
    s += "; dumped " + path.toStdString();
  }
  return s;
}

void requirePattern(QQuickWindow& window, QQuickItem& item, const Pattern& p)
{
  QImage image;
  QRectF rect;
  const bool matched = eventually([&] {
    window.update();
    image = window.grabWindow();
    rect = windowRect(item);
    return hasPattern(image, rect, grabScale(window, image), p);
  });
  INFO(report(window, image, rect));
  REQUIRE(matched);
}

void requireBlank(QQuickWindow& window, QQuickItem& item, const QColor& bg)
{
  QImage image;
  QRectF rect;
  const bool blank = eventually([&] {
    window.update();
    image = window.grabWindow();
    rect = windowRect(item);
    return isBlank(image, rect, grabScale(window, image), bg);
  });
  INFO(report(window, image, rect));
  REQUIRE(blank);
}

struct StopOnExit
{
  Transport::TransportInterface& transport;
  ~StopOnExit()
  {
    transport.requestStop();
    spin(100);
  }
};

//! Everything a case needs before it can talk about pixels. Skips, saying
//! which prerequisite is missing.
bool gpuAvailable(std::string& why)
{
  if(qgetenv("QT_QUICK_BACKEND") == "software")
  {
    why = "TextureSource requires QRhi; unset QT_QUICK_BACKEND=software and use "
          "Xvfb with GL";
    return false;
  }
  QOpenGLContext gl;
  if(!gl.create())
  {
    why = "No OpenGL context is available; run under a GL-capable display or Xvfb";
    return false;
  }
  QOffscreenSurface surface;
  surface.setFormat(gl.format());
  surface.create();
  if(!surface.isValid() || !gl.makeCurrent(&surface))
  {
    why = "The display cannot make an OpenGL offscreen surface current";
    return false;
  }
  gl.doneCurrent();
  return true;
}

void requireGpu()
{
  std::string why;
  if(!gpuAvailable(why))
    SKIP(why);
}

struct Env
{
  QTemporaryDir settings;
  std::unique_ptr<EnvironmentGuard> config;
  std::unique_ptr<EnvironmentGuard> backend;
  std::unique_ptr<EnvironmentGuard> renderLoop;
  std::unique_ptr<EnvironmentGuard> audio;

  Env()
  {
    REQUIRE(settings.isValid());
    config
        = std::make_unique<EnvironmentGuard>("XDG_CONFIG_HOME", settings.path().toUtf8());
    backend = std::make_unique<EnvironmentGuard>("QSG_RHI_BACKEND", "opengl");
    // As in the application: graph previews run on score's GUI render loop.
    renderLoop = std::make_unique<EnvironmentGuard>("QSG_RENDER_LOOP", "basic");
    audio = std::make_unique<EnvironmentGuard>("SCORE_AUDIO_BACKEND", "dummy");
  }
};

// One case cannot run in this process: it needs a device pixel ratio, which
// QGuiApplication fixes at construction. It runs in a child copy of this very
// binary, selected by a hidden Catch2 tag, and reports through a marker on
// stdout rather than an exit code, so that "the prerequisite was missing" and
// "the assertion failed" stay distinguishable across the process boundary.

struct ChildRun
{
  int exitCode{-1};
  bool crashed{true};
  QString log;
};

ChildRun runChild(const char* tag, const QList<QPair<QString, QString>>& extraEnv)
{
  const QString self = QFileInfo{QStringLiteral("/proc/self/exe")}.canonicalFilePath();
  if(self.isEmpty())
    SKIP("cannot re-exec this binary: /proc/self/exe is unavailable");

  auto qenv = QProcessEnvironment::systemEnvironment();
  for(const auto& [k, v] : extraEnv)
    qenv.insert(k, v);

  QProcess child;
  child.setProcessEnvironment(qenv);
  child.setProcessChannelMode(QProcess::MergedChannels);
  child.start(self, {QString::fromLatin1(tag)});

  ChildRun r;
  if(!child.waitForStarted(30000) || !child.waitForFinished(300000))
  {
    child.kill();
    child.waitForFinished(5000);
    r.log = QString::fromUtf8(child.readAll());
    return r;
  }
  r.log = QString::fromUtf8(child.readAll());
  r.crashed = child.exitStatus() != QProcess::NormalExit;
  r.exitCode = child.exitCode();
  return r;
}

void say(const char* marker, const std::string& detail = {})
{
  std::printf("%s %s\n", marker, detail.c_str());
  std::fflush(stdout);
}
}

// 1. A texture source that is laid out -- anchors.fill inside a Layout -- so
//    its size changes on every window resize and every content-height change,
//    through fractional intermediate values.
//
//    score decides the resolution of the WHOLE graph from the item's physical
//    colour-buffer size, once, in TextureSourceRenderer::rebuild, and nothing
//    in PreviewNode resizes it afterwards: a resize works because Qt calls
//    QQuickRhiItemRenderer::initialize on every synchronize and score's
//    initialize forces a full teardown and re-registration.
//
//    So: assert what such a UI needs -- the colour buffer follows the item,
//    and the rendered content fills it -- not how that comes about.
TEST_CASE(
    "a live texture source follows integer and fractional resizes",
    "[integration][gfx][js][preview]")
{
#if !defined(SCORE_HAS_GPU_JS)
  SKIP("This build has no native Javascript GPU support (SCORE_HAS_GPU_JS)");
#else
  Env env;
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    requireGpu();

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    auto* process = addSource(ctx, *doc, interval, kSource, rgby);
    auto* execution = doc->context().findPlugin<Execution::DocumentPlugin>();
    REQUIRE(execution != nullptr);
    auto& transport = execution->executionController().transport();

    QQuickWindow window;
    window.setColor(Qt::black);
    window.resize(640, 400);
    QQmlEngine engine;
    auto* root = buildUi(engine, window, QStringLiteral(R"QML(import QtQuick
import Score.UI
Item {
  width: 640; height: 400
  TextureSource {
    objectName: "tex"
    x: 0; y: 0
    width: 200; height: 120
    process: "%1"
    port: 0
  }
}
)QML")
                                             .arg(kSource));
    auto* tex = root->findChild<QQuickItem*>(QStringLiteral("tex"));
    REQUIRE(tex != nullptr);

    window.show();
    REQUIRE(eventually([&] { return window.isExposed(); }));
    REQUIRE(window.rendererInterface()->graphicsApi() == QSGRendererInterface::OpenGL);
    StopOnExit stop{transport};

    transport.requestPlay();
    REQUIRE(eventually([&] {
      return execution->isPlaying() && process->executing()
             && textureOutlet(*process).graphicsPort().node >= 0;
    }));
    requirePattern(window, *tex, rgby);

    const auto colorBuffer
        = [&] { return tex->property("effectiveColorBufferSize").toSize(); };
    // The contract score reads: Qt hands it the PHYSICAL size, int-truncated,
    // times the device pixel ratio (here 1).
    REQUIRE(eventually([&] { return colorBuffer() == QSize(200, 120); }));

    // Grow: a side panel closing, or the window being maximised.
    tex->setWidth(420);
    tex->setHeight(260);
    REQUIRE(eventually([&] { return colorBuffer() == QSize(420, 260); }));
    requirePattern(window, *tex, rgby);

    // Shrink back past the first size: a stale buffer is as wrong the other
    // way round, and the content must not stay cropped to the big extent.
    tex->setWidth(120);
    tex->setHeight(72);
    REQUIRE(eventually([&] { return colorBuffer() == QSize(120, 72); }));
    requirePattern(window, *tex, rgby);

    // Fractional, the size a laid-out preview actually produces: a height of
    // 350 at 1280/720 is 622.22 wide, and the intermediate values a Layout
    // walks through are not integers either.
    tex->setWidth(311.4);
    tex->setHeight(175.2);
    REQUIRE(eventually([&] { return colorBuffer() == QSize(311, 175); }));
    requirePattern(window, *tex, rgby);

    // ...and a sub-pixel change, which must not be mistaken for a new size.
    tex->setWidth(311.8);
    REQUIRE(eventually([&] { return colorBuffer() == QSize(311, 175); }));
    requirePattern(window, *tex, rgby);
  });
#endif
}

// 2. The Retina case. Qt sizes the colour buffer as
//    QSize(int(w), int(h)) * effectiveDevicePixelRatio and score takes that
//    physical size as the render resolution of the whole graph, so a 2x screen
//    quadruples the pixel work -- which is why a UI that wants a fixed
//    resolution pins fixedColorBufferWidth/Height and divides the item size by
//    the ratio. Neither is a correctness fix, and both rely on score actually
//    rendering into the whole physical buffer.
namespace
{
constexpr auto kDprTag = "[.dpr-child]";
constexpr auto kDprOk = "DPR-CHILD-OK";
constexpr auto kDprSkip = "DPR-CHILD-SKIP";
}

TEST_CASE("the 2x child of the device-pixel-ratio case", kDprTag)
{
#if !defined(SCORE_HAS_GPU_JS)
  say(kDprSkip, "no SCORE_HAS_GPU_JS");
#else
  Env env;
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    std::string why;
    if(!gpuAvailable(why))
    {
      say(kDprSkip, why);
      return;
    }

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    auto* process = addSource(ctx, *doc, interval, kSource, rgby);
    auto* execution = doc->context().findPlugin<Execution::DocumentPlugin>();
    REQUIRE(execution != nullptr);
    auto& transport = execution->executionController().transport();

    QQuickWindow window;
    window.setColor(Qt::black);
    window.resize(400, 260);
    QQmlEngine engine;
    auto* root = buildUi(engine, window, QStringLiteral(R"QML(import QtQuick
import Score.UI
Item {
  width: 400; height: 260
  TextureSource {
    objectName: "tex"
    x: 0; y: 0
    width: 160; height: 100
    process: "%1"
    port: 0
  }
}
)QML")
                                             .arg(kSource));
    auto* tex = root->findChild<QQuickItem*>(QStringLiteral("tex"));
    REQUIRE(tex != nullptr);

    window.show();
    REQUIRE(eventually([&] { return window.isExposed(); }));
    if(window.effectiveDevicePixelRatio() != 2.)
    {
      say(kDprSkip,
          "QT_SCALE_FACTOR=2 gave a device pixel ratio of "
              + std::to_string(window.effectiveDevicePixelRatio()));
      return;
    }
    StopOnExit stop{transport};

    transport.requestPlay();
    REQUIRE(eventually([&] {
      return execution->isPlaying() && process->executing()
             && textureOutlet(*process).graphicsPort().node >= 0;
    }));

    const auto colorBuffer
        = [&] { return tex->property("effectiveColorBufferSize").toSize(); };

    // The backing texture is the logical size times the ratio, and score must
    // render into all of it.
    REQUIRE(eventually([&] { return colorBuffer() == QSize(320, 200); }));
    requirePattern(window, *tex, rgby);

    // A resize at 2x must land on the physical size, not the logical one.
    tex->setWidth(240);
    tex->setHeight(150);
    REQUIRE(eventually([&] { return colorBuffer() == QSize(480, 300); }));
    requirePattern(window, *tex, rgby);

    say(kDprOk);
  });
#endif
}

TEST_CASE(
    "a texture source honours a device pixel ratio of 2",
    "[integration][gfx][js][preview]")
{
  const auto r = runChild(kDprTag, {{"QT_SCALE_FACTOR", "2"}});
  INFO(r.log.toStdString());
  if(r.log.contains(QString::fromLatin1(kDprSkip)))
    SKIP("the 2x child could not run: see its output");
  CHECK_FALSE(r.crashed);
  CHECK(r.exitCode == 0);
  REQUIRE(r.log.contains(QString::fromLatin1(kDprOk)));
}

// 3. Two preview panels showing one process at once: each panel has its own
//    TextureSource on the same GFX process, and `active` takes the invisible
//    one away again.
//
//    Two sinks on one node is two render lists by construction
//    (GfxContext::add_preview_output -> Graph::createSingleRenderList) and each
//    render list calls initState on every node of the chain, so a Javascript
//    GPU process ends up with one Qt Quick runtime per render list.
//
//    What such a UI needs: both previews show the process, each at ITS OWN
//    resolution (a shared runtime would give both the same one); the process
//    answers its UI once per message rather than once per panel, because a
//    render list is a render of it and not another copy of it; and whichever
//    preview is left keeps rendering AND answering when the other goes away.
TEST_CASE(
    "two texture sources on one process both render it",
    "[integration][gfx][js][preview]")
{
#if !defined(SCORE_HAS_GPU_JS)
  SKIP("This build has no native Javascript GPU support (SCORE_HAS_GPU_JS)");
#else
  Env env;
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    requireGpu();

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    auto* process = addSource(ctx, *doc, interval, kSource, rgby);
    auto* execution = doc->context().findPlugin<Execution::DocumentPlugin>();
    REQUIRE(execution != nullptr);
    auto& transport = execution->executionController().transport();

    QQuickWindow window;
    window.setColor(Qt::black);
    window.resize(640, 400);
    QQmlEngine engine;
    // The first panel. The second one opens later, while this one is already
    // rendering: that is how an operator gets two of them, and it pins down
    // which of the two render lists is the older one.
    auto* texA = buildUi(engine, window, QStringLiteral(R"QML(import QtQuick
import Score.UI
TextureSource {
  objectName: "texA"
  x: 0; y: 0; width: 240; height: 160
  process: "%1"; port: 0
}
)QML")
                                             .arg(kSource));

    window.show();
    REQUIRE(eventually([&] { return window.isExposed(); }));
    REQUIRE(window.rendererInterface()->graphicsApi() == QSGRendererInterface::OpenGL);
    StopOnExit stop{transport};

    transport.requestPlay();
    REQUIRE(eventually([&] {
      return execution->isPlaying() && process->executing()
             && textureOutlet(*process).graphicsPort().node >= 0;
    }));

    requirePattern(window, *texA, rgby);

    // One panel, one answer per message: the baseline the second panel must
    // not change.
    std::unique_ptr<QQuickItem> ui{process->createItemForUI(doc->context())};
    REQUIRE(ui != nullptr);
    const auto echoes = [&ui] { return ui->property("echoes").toInt(); };
    REQUIRE(echoes() == 0);
    process->uiToExecution(QVariant{QStringLiteral("ping")});
    REQUIRE(eventually([&] { return echoes() >= 1; }));
    spin(300);
    INFO("answers to one message, one preview: " << echoes());
    REQUIRE(echoes() == 1);

    // The second panel, a different size, on the same process and port.
    auto* texB = buildUi(engine, window, QStringLiteral(R"QML(import QtQuick
import Score.UI
TextureSource {
  objectName: "texB"
  x: 320; y: 200; width: 300; height: 180
  process: "%1"; port: 0
}
)QML")
                                             .arg(kSource));
    requirePattern(window, *texA, rgby);
    requirePattern(window, *texB, rgby);

    // Each render list renders the process at its own size: the graph
    // resolution comes from the item's colour buffer, so two items of different
    // sizes previewing one process are two independently-sized renders of it --
    // the second must neither inherit nor overwrite the first's.
    const auto colorBuffer
        = [](QQuickItem& item) { return item.property("effectiveColorBufferSize"); };
    REQUIRE(eventually([&] { return colorBuffer(*texA).toSize() == QSize(240, 160); }));
    REQUIRE(eventually([&] { return colorBuffer(*texB).toSize() == QSize(300, 180); }));

    // Two previews are two renders of one process, not two processes: the
    // script answers the UI once per message, however many panels show it.
    // Each render list runs its own instance of the script (it has its own Qt
    // Quick scene), so without an owner for the process's outputs opening the
    // second panel doubles this -- and a mapping whose script drives values or
    // replies to its UI would double everything with it.
    process->uiToExecution(QVariant{QStringLiteral("ping-two")});
    REQUIRE(eventually([&] { return echoes() >= 2; }));
    spin(400);
    INFO("answers to two messages, the second with two previews: " << echoes());
    REQUIRE(echoes() == 2);

    // Now take the FIRST panel away. Its render list is the one that has been
    // answering since before the second existed, so this is the handover, and
    // it is the way a UI swaps panels: the one being closed is the one that
    // was showing. The survivor has to keep rendering...
    texA->deleteLater();
    spin(300);
    requirePattern(window, *texB, rgby);

    // ...and has to take the answers over. A process that goes silent because
    // the panel that happened to own its outputs was closed is no better than
    // one that answers twice.
    process->uiToExecution(QVariant{QStringLiteral("ping-alone")});
    REQUIRE(eventually([&] { return echoes() >= 3; }));
    spin(300);
    INFO("answers to three messages, one preview left: " << echoes());
    REQUIRE(echoes() == 3);
  });
#endif
}

// 4. A source rendered with `visible: false` and sampled through
//    `Texture { sourceItem: ... }` or a
//    `ShaderEffectSource { hideSource: true }`. An invisible item that must
//    still render is the normal case whenever the source feeds something else
//    instead of being shown directly.
//
//    Then the process underneath is destroyed and rebuilt, as switching the
//    source's backend or input mode does. The document is stopped for the
//    swap.
TEST_CASE(
    "a hidden texture source renders, and follows its process being rebuilt",
    "[integration][gfx][js][preview]")
{
#if !defined(SCORE_HAS_GPU_JS)
  SKIP("This build has no native Javascript GPU support (SCORE_HAS_GPU_JS)");
#else
  Env env;
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    requireGpu();

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    QPointer<JS::ProcessModel> process = addSource(ctx, *doc, interval, kSource, rgby);
    auto* execution = doc->context().findPlugin<Execution::DocumentPlugin>();
    REQUIRE(execution != nullptr);
    auto& transport = execution->executionController().transport();

    QQuickWindow window;
    window.setColor(Qt::black);
    window.resize(640, 400);
    QQmlEngine engine;
    // The arrangement: the source is invisible and what the user sees is a
    // ShaderEffectSource with hideSource: true.
    auto* root = buildUi(engine, window, QStringLiteral(R"QML(import QtQuick
import Score.UI
Item {
  width: 640; height: 400
  TextureSource {
    id: hidden
    objectName: "tex"
    x: 0; y: 0; width: 320; height: 180
    process: "%1"; port: 0
    visible: false
  }
  ShaderEffectSource {
    objectName: "mirror"
    x: 0; y: 200; width: 320; height: 180
    sourceItem: hidden
    hideSource: true
  }
}
)QML")
                                             .arg(kSource));
    auto* tex = root->findChild<QQuickItem*>(QStringLiteral("tex"));
    auto* mirror = root->findChild<QQuickItem*>(QStringLiteral("mirror"));
    REQUIRE(tex != nullptr);
    REQUIRE(mirror != nullptr);
    REQUIRE_FALSE(tex->isVisible());

    window.show();
    REQUIRE(eventually([&] { return window.isExposed(); }));
    REQUIRE(window.rendererInterface()->graphicsApi() == QSGRendererInterface::OpenGL);
    StopOnExit stop{transport};

    transport.requestPlay();
    REQUIRE(eventually([&] {
      return execution->isPlaying() && process->executing()
             && textureOutlet(*process).graphicsPort().node >= 0;
    }));

    // The hidden source rendered: its content is on screen through the mirror.
    requirePattern(window, *mirror, rgby);
    // ...and nothing was painted where the hidden item itself sits.
    requireBlank(window, *tex, Qt::black);

    // Backend switch: stop, throw the process away, build a new one under the
    // same label with a different picture, play again.
    transport.requestStop();
    REQUIRE(eventually([&] { return !execution->isPlaying(); }));

    // The old process really is gone, not merely reprogrammed. score recycles
    // process ids, so the QPointer going null is the observable, not the id.
    Scenario::RemoveProcess(interval, process->id());
    spin(200);
    REQUIRE(process.isNull());
    requireBlank(window, *mirror, Qt::black);

    QPointer<JS::ProcessModel> rebuilt = addSource(ctx, *doc, interval, kSource, cmwg);

    transport.requestPlay();
    REQUIRE(eventually([&] {
      return execution->isPlaying() && rebuilt->executing()
             && textureOutlet(*rebuilt).graphicsPort().node >= 0;
    }));
    requirePattern(window, *mirror, cmwg);
  });
#endif
}

// 5. The two ways a UI points a TextureSource at nothing: `process:` bound to
//    the result of a lookup, or to
//    `currentProcess ? currentProcess.videoMapperLabel : ""`, so an empty or
//    stale name is a normal transient state, not a programming error. `port:`
//    is an index in both, and the number of outlets changes when the script
//    does.
//
//    Neither may crash, neither may keep showing the last good frame, and
//    both must recover when the binding resolves.
TEST_CASE(
    "a texture source naming nothing shows nothing, and recovers",
    "[integration][gfx][js][preview]")
{
#if !defined(SCORE_HAS_GPU_JS)
  SKIP("This build has no native Javascript GPU support (SCORE_HAS_GPU_JS)");
#else
  Env env;
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    requireGpu();

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    auto* process = addSource(ctx, *doc, interval, kSource, rgby);
    auto* execution = doc->context().findPlugin<Execution::DocumentPlugin>();
    REQUIRE(execution != nullptr);
    auto& transport = execution->executionController().transport();

    QQuickWindow window;
    window.setColor(Qt::black);
    window.resize(640, 400);
    QQmlEngine engine;
    auto* root = buildUi(engine, window, QStringLiteral(R"QML(import QtQuick
import Score.UI
Item {
  width: 640; height: 400
  TextureSource {
    objectName: "tex"
    x: 0; y: 0; width: 320; height: 200
    process: "no-such-process"
    port: 0
  }
}
)QML"));
    auto* tex = root->findChild<QQuickItem*>(QStringLiteral("tex"));
    REQUIRE(tex != nullptr);

    window.show();
    REQUIRE(eventually([&] { return window.isExposed(); }));
    REQUIRE(window.rendererInterface()->graphicsApi() == QSGRendererInterface::OpenGL);
    StopOnExit stop{transport};

    transport.requestPlay();
    REQUIRE(eventually([&] {
      return execution->isPlaying() && process->executing()
             && textureOutlet(*process).graphicsPort().node >= 0;
    }));

    // A name that matches no process: nothing rendered, and the process is
    // still running for whoever does ask for it.
    requireBlank(window, *tex, Qt::black);
    CHECK(textureOutlet(*process).graphicsPort().node >= 0);

    // An empty name, the `currentProcess ? ... : ""` binding unresolved.
    REQUIRE(tex->setProperty("process", QString{}));
    spin(200);
    requireBlank(window, *tex, Qt::black);

    // The binding resolves: it must start working with no further help.
    REQUIRE(tex->setProperty("process", QString::fromLatin1(kSource)));
    requirePattern(window, *tex, rgby);

    // An index past the end of the outlet list. The last good frame must not
    // survive it: a stale preview is worse than a black one, because the
    // operator cannot tell that it is stale.
    REQUIRE(tex->setProperty("port", 5));
    requireBlank(window, *tex, Qt::black);
    REQUIRE(tex->setProperty("port", 0));
    requirePattern(window, *tex, rgby);

    // ...and a port named after nothing at all behaves the same way.
    REQUIRE(tex->setProperty("port", QStringLiteral("no-such-port")));
    requireBlank(window, *tex, Qt::black);
    REQUIRE(tex->setProperty("port", QStringLiteral("Output")));
    requirePattern(window, *tex, rgby);
  });
#endif
}
