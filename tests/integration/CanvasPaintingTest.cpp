#include <JS/Qml/Utils.hpp>
#include <Library/LibrarySettings.hpp>

#include <score/application/ApplicationContext.hpp>

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QVariantMap>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

#include <cmath>

#include <algorithm>
#include <memory>

namespace
{
struct Environment
{
  const char* name;
  QByteArray previous;
  bool existed;
  Environment(const char* name, const QByteArray& value)
      : name{name}
      , previous{qgetenv(name)}
      , existed{qEnvironmentVariableIsSet(name)}
  {
    qputenv(name, value);
  }
  ~Environment()
  {
    if(existed)
      qputenv(name, previous);
    else
      qunsetenv(name);
  }
};

template <typename Predicate>
bool eventually(Predicate&& predicate)
{
  QElapsedTimer timer;
  timer.start();
  do
  {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    if(predicate())
      return true;
    QThread::msleep(2);
  } while(timer.elapsed() < 5000);
  return false;
}

bool near(const QColor& a, const QColor& b)
{
  return std::abs(a.red() - b.red()) <= 2 && std::abs(a.green() - b.green()) <= 2
         && std::abs(a.blue() - b.blue()) <= 2 && std::abs(a.alpha() - b.alpha()) <= 2;
}

QVariant plain(const QVariant& value)
{
  if(value.metaType() == QMetaType::fromType<QJSValue>())
    return value.value<QJSValue>().toVariant();
  return value;
}

// Untyped QML functions take and return QVariant, unlike the removed C++ item.
template <typename... Args>
QVariant invoke(QQuickItem& item, const char* method, const Args&... args)
{
  QVariant result;
  INFO("QML method: " << method);
  REQUIRE(
      QMetaObject::invokeMethod(
          &item, method, Qt::DirectConnection, qReturnArg(result),
          QVariant::fromValue(args)...));
  return plain(result);
}

QVariantMap style(
    const QString& tool, const QString& color = "#ffffff", double width = 40.,
    double opacity = 1., bool filled = false)
{
  return {
      {"tool", tool},
      {"color", color},
      {"width", width},
      {"opacity", opacity},
      {"filled", filled}};
}

QVariantMap command(QVariantMap config, std::initializer_list<QPointF> points)
{
  QVariantList serialized;
  for(const auto& point : points)
    serialized.push_back(QVariantList{point.x(), point.y()});
  config["points"] = serialized;
  return config;
}

void load(QQuickItem& paint, const QVariantList& commands)
{
  invoke(paint, "loadCommands", commands);
}

QString presetsRoot()
{
  if(const auto path = qEnvironmentVariable("SCORE_JS_PRESETS_DIR"); !path.isEmpty())
    return QFileInfo{path}.isDir() ? path : QString{};
  const auto& library = score::AppContext().settings<Library::Settings::Model>();
  const QStringList candidates{
      library.getDefaultLibraryPath() + "/Presets/Javascript",
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
          + "/ossia/score/packages/default/Presets/Javascript"};
  for(const auto& path : candidates)
    if(QFileInfo{path}.isDir())
      return path;
  return {};
}

// The wrapper only records public signals; all painting and interaction remain
// in the installed production components. No Qt Test dependency is needed.
struct SignalLog
{
  QQuickItem* item;
  const char* property;

  QVariantList values() const { return plain(item->property(property)).toList(); }
  bool empty() const { return values().empty(); }
  qsizetype size() const { return values().size(); }
  QVariantList front() const { return {values().front()}; }
  QVariantList back() const { return {values().back()}; }
  void clear() { REQUIRE(item->setProperty(property, QVariantList{})); }
};

struct Pixel
{
  QPointF document;
  QColor color;
};

struct Canvas
{
  JS::JsUtils util;
  QQmlEngine engine;
  QQuickWindow window;
  std::unique_ptr<QQuickItem> item;
  bool view;

  explicit Canvas(bool view = false)
      : view{view}
  {
    engine.rootContext()->setContextProperty("Util", &util);
    const auto root = presetsRoot();
    if(root.isEmpty())
      SKIP("Javascript presets are not installed; set SCORE_JS_PRESETS_DIR");
    const QDir presets{root + "/canvas-painter"};
    const auto path = presets.filePath(view ? "PaintView.qml" : "PaintSurface.qml");
    INFO("Production QML: " << path.toStdString());
    REQUIRE(QFileInfo::exists(path));
    QQmlComponent component{&engine};
    const QByteArray wrapper = view ? R"QML(
import QtQuick
import "."
PaintView {
    property var testSamples: []
    property var testCommits: []
    property var testGestures: []
    onSampled: function(color) { testSamples = testSamples.concat([color]); }
    onCommitted: function(command) { testCommits = testCommits.concat([command]); }
    onGesture: function(event) { testGestures = testGestures.concat([event]); }
}
)QML"
                                    : R"QML(
import QtQuick
import "."
PaintSurface {
    property var testSamples: []
    onColorSampled: function(color) { testSamples = testSamples.concat([color]); }
}
)QML";
    component.setData(
        wrapper, QUrl::fromLocalFile(presets.filePath("PaintingTest.qml")));
    REQUIRE(eventually([&] { return !component.isLoading(); }));
    INFO(component.errorString().toStdString());
    REQUIRE(component.isReady());
    item.reset(qobject_cast<QQuickItem*>(component.create()));
    REQUIRE(item != nullptr);
    window.setColor(Qt::black);
    item->setParentItem(window.contentItem());
    REQUIRE(
        item->setProperty(view ? "backgroundColor" : "fillColor", QColor{Qt::black}));
    resize({640, 360});
    window.show();
    REQUIRE(eventually([&] { return window.isExposed(); }));
  }

  void resize(QSize size)
  {
    window.resize(size);
    item->setSize(size);
  }

  QPointF host(QPointF document) const
  {
    if(!view)
      return {
          document.x() * item->width() / 1280., document.y() * item->height() / 720.};
    const double width = std::min(item->width(), item->height() * 1280. / 720.);
    const double height = width * 720. / 1280.;
    // anchors.centerIn aligns each item's center line to logical pixels.
    return {
        std::round(item->width() / 2.) - std::round(width / 2.)
            + document.x() * width / 1280.,
        std::round(item->height() / 2.) - std::round(height / 2.)
            + document.y() * height / 720.};
  }

  QColor pixel(const QImage& frame, QPointF position) const
  {
    return frame.pixelColor(
        std::clamp(
            int(position.x() * frame.width() / window.width()), 0, frame.width() - 1),
        std::clamp(
            int(position.y() * frame.height() / window.height()), 0,
            frame.height() - 1));
  }

  void checkPixels(std::initializer_list<Pixel> pixels)
  {
    QImage frame;
    const bool matched = eventually([&] {
      frame = window.grabWindow();
      if(frame.isNull())
        return false;
      return std::all_of(pixels.begin(), pixels.end(), [&](const Pixel& expected) {
        return near(pixel(frame, host(expected.document)), expected.color);
      });
    });
    REQUIRE_FALSE(frame.isNull());
    for(const auto& expected : pixels)
    {
      const auto actual = pixel(frame, host(expected.document));
      INFO(
          "Document pixel " << expected.document.x() << "," << expected.document.y()
                            << ": expected "
                            << expected.color.name(QColor::HexArgb).toStdString()
                            << ", displayed "
                            << actual.name(QColor::HexArgb).toStdString());
      CHECK(near(actual, expected.color));
    }
    REQUIRE(matched);
  }

  void frames(int count = 3)
  {
    QObject receiver;
    int swapped = 0;
    QObject::connect(
        &window, &QQuickWindow::frameSwapped, &receiver, [&] { ++swapped; });
    for(int index = 0; index < count; ++index)
    {
      const auto previous = swapped;
      window.update();
      REQUIRE(eventually([&] { return swapped > previous; }));
    }
  }

  QColor sample(QPointF point)
  {
    SignalLog sampled{item.get(), "testSamples"};
    sampled.clear();
    invoke(*item, "sampleColor", point.x(), point.y());
    REQUIRE(eventually([&] { return !sampled.empty(); }));
    REQUIRE(sampled.size() == 1);
    return sampled.front().front().value<QColor>();
  }

  void checkSample(QPointF point, QColor expected)
  {
    const auto actual = sample(point);
    INFO(
        "Expected sampled " << expected.name(QColor::HexArgb).toStdString() << ", got "
                            << actual.name(QColor::HexArgb).toStdString());
    CHECK(near(actual, expected));
  }

  void mouse(QEvent::Type type, QPointF document)
  {
    const auto position = host(document);
    const auto button = type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
    const auto buttons
        = type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton;
    QMouseEvent event{
        type,   position, position,      window.mapToGlobal(position.toPoint()),
        button, buttons,  Qt::NoModifier};
    QCoreApplication::sendEvent(&window, &event);
  }
};

template <typename Function>
void withCanvasApp(Function&& function)
{
  if(qgetenv("QT_QUICK_BACKEND") == "software")
    SKIP("Canvas2D requires QRhi, not the software Qt Quick backend");
  QTemporaryDir settings;
  REQUIRE(settings.isValid());
  Environment config{"XDG_CONFIG_HOME", settings.path().toUtf8()};
  Environment backend{"QSG_RHI_BACKEND", "opengl"};
  Environment renderLoop{"QSG_RENDER_LOOP", "basic"};

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    QOpenGLContext probe;
    if(!probe.create())
      SKIP("An OpenGL-capable display or Xvfb is required");
    QOffscreenSurface surface;
    surface.setFormat(probe.format());
    surface.create();
    if(!surface.isValid() || !probe.makeCurrent(&surface))
      SKIP("The display cannot make an OpenGL offscreen surface current");
    probe.doneCurrent();
    function();
  });
}
}

TEST_CASE(
    "Canvas2D tools retain their document geometry",
    "[integration][js][gfx][painting][gui]")
{
  withCanvasApp([] {
    Canvas canvas;
    auto& paint = *canvas.item;

    SECTION("single point brush is a filled disk, not an empty path")
    {
      load(paint, {command(style("brush", "#ff0000", 100.), {{320., 180.}})});
      canvas.checkPixels(
          {{{320., 180.}, Qt::red},
           {{360., 180.}, Qt::red},
           {{380., 180.}, Qt::black},
           {{320., 240.}, Qt::black}});
    }
    SECTION("line has round caps and keeps its document width")
    {
      load(
          paint, {command(style("line", "#00ff00", 80.), {{320., 360.}, {960., 360.}})});
      canvas.checkPixels(
          {{{640., 360.}, Qt::green},
           {{640., 390.}, Qt::green},
           {{290., 360.}, Qt::green},
           {{640., 410.}, Qt::black},
           {{260., 360.}, Qt::black}});
    }
    SECTION("freehand grows from a dot and retains intermediate vertices")
    {
      invoke(paint, "beginStroke", style("brush", "#00ff00", 60.), 160., 540.);
      canvas.checkPixels({{{160., 540.}, Qt::green}, {{400., 360.}, Qt::black}});
      invoke(paint, "extendStroke", 640., 180.);
      canvas.checkPixels({{{400., 360.}, Qt::green}, {{880., 360.}, Qt::black}});
      invoke(paint, "extendStroke", 1120., 540.);
      canvas.checkPixels(
          {{{640., 180.}, Qt::green},
           {{400., 360.}, Qt::green},
           {{880., 360.}, Qt::green},
           {{640., 540.}, Qt::black}});
      const auto committed = invoke(paint, "endStroke").toMap();
      CHECK(
          committed.value("points").toList()
          == QVariantList{
              QVariantList{160., 540.}, QVariantList{640., 180.},
              QVariantList{1120., 540.}});
      load(paint, {});
      load(paint, {committed});
      canvas.checkPixels(
          {{{400., 360.}, Qt::green},
           {{880., 360.}, Qt::green},
           {{640., 540.}, Qt::black}});
    }
    SECTION("rectangle normalizes reverse drags and distinguishes fill from outline")
    {
      const auto points = {QPointF{960., 540.}, QPointF{320., 180.}};
      load(paint, {command(style("rectangle", "#0000ff", 40.), points)});
      canvas.checkPixels(
          {{{320., 360.}, Qt::blue},
           {{640., 180.}, Qt::blue},
           {{640., 360.}, Qt::black},
           {{260., 360.}, Qt::black}});
      load(paint, {command(style("rectangle", "#0000ff", 40., 1., true), points)});
      canvas.checkPixels(
          {{{640., 360.}, Qt::blue},
           {{360., 220.}, Qt::blue},
           {{260., 360.}, Qt::black}});
    }
    SECTION("ellipse is curved, normalized, and supports outlined and filled modes")
    {
      const auto points = {QPointF{960., 540.}, QPointF{320., 180.}};
      load(paint, {command(style("ellipse", "#ffffff", 40.), points)});
      canvas.checkPixels(
          {{{320., 360.}, Qt::white},
           {{640., 180.}, Qt::white},
           {{640., 360.}, Qt::black},
           {{340., 200.}, Qt::black}});
      load(paint, {command(style("ellipse", "#ffffff", 40., 1., true), points)});
      canvas.checkPixels(
          {{{640., 360.}, Qt::white},
           {{640., 240.}, Qt::white},
           {{340., 200.}, Qt::black}});
    }
  });
}

TEST_CASE(
    "Canvas2D translucent previews commit and reload without accumulating alpha",
    "[integration][js][gfx][painting][gui]")
{
  withCanvasApp([] {
    Canvas canvas;
    auto& paint = *canvas.item;
    const auto rectangle = command(
        style("rectangle", "#ffffff", 40., 0.5, true), {{160., 90.}, {1120., 630.}});
    load(paint, {rectangle});
    canvas.checkPixels({{{640., 360.}, QColor{128, 128, 128}}});
    canvas.checkSample({640., 360.}, QColor{128, 128, 128});

    load(paint, {});
    const auto config = style("rectangle", "#80ff40", 40., 0.25, true);
    invoke(paint, "beginStroke", config, 160., 90.);
    invoke(paint, "extendStroke", 1120., 630.);
    canvas.checkPixels({{{640., 360.}, QColor{32, 64, 16}}});
    canvas.checkSample({640., 360.}, QColor{32, 64, 16});
    for(int frame = 0; frame < 4; ++frame)
    {
      canvas.frames(1);
      canvas.checkPixels({{{640., 360.}, QColor{32, 64, 16}}});
    }
    invoke(paint, "cancelStroke");
    canvas.checkPixels({{{640., 360.}, Qt::black}});
    CHECK(invoke(paint, "endStroke").toMap().isEmpty());

    invoke(paint, "beginStroke", config, 160., 90.);
    invoke(paint, "extendStroke", 640., 360.);
    invoke(paint, "extendStroke", 1120., 630.);
    const auto committed = invoke(paint, "endStroke").toMap();
    REQUIRE(committed.value("tool").toString() == "rectangle");
    CHECK(
        committed.value("points").toList()
        == QVariantList{QVariantList{160., 90.}, QVariantList{1120., 630.}});
    canvas.checkPixels({{{640., 360.}, QColor{32, 64, 16}}});
    load(paint, {});
    canvas.checkPixels({{{640., 360.}, Qt::black}});
    load(paint, {committed});
    canvas.checkPixels({{{640., 360.}, QColor{32, 64, 16}}});
    canvas.checkSample({640., 360.}, QColor{32, 64, 16});
    // Replaying the same document and repainting after a resize must not blend it twice.
    load(paint, {committed});
    canvas.resize({320, 180});
    canvas.frames();
    canvas.checkPixels({{{640., 360.}, QColor{32, 64, 16}}});
    canvas.checkSample({640., 360.}, QColor{32, 64, 16});
  });
}

TEST_CASE(
    "Canvas2D brush feather softens inward with straight alpha",
    "[integration][js][gfx][painting][gui]")
{
  withCanvasApp([] {
    Canvas canvas;
    auto& paint = *canvas.item;
    REQUIRE(paint.setProperty("fillColor", QColor{Qt::transparent}));
    QVariantMap stroke;
    SECTION("dot")
    {
      stroke = command(style("brush", "#80ff0000", 200., 0.5), {{640., 360.}});
    }
    SECTION("polyline")
    {
      stroke = command(
          style("brush", "#80ff0000", 200., 0.5),
          {{320., 360.}, {640., 360.}, {960., 360.}});
    }

    // Legacy commands omit feather; explicit zero must keep their hard edge.
    for(int legacy = 0; legacy < 2; ++legacy)
    {
      if(legacy)
        stroke["feather"] = 0.;
      load(paint, {stroke});
      canvas.checkPixels(
          {{{640., 360.}, QColor{64, 0, 0}},
           {{640., 440.}, QColor{64, 0, 0}},
           {{640., 472.}, Qt::black}});
      canvas.checkSample({640., 440.}, QColor{255, 0, 0, 64});
    }

    stroke["feather"] = 0.5;
    load(paint, {stroke});
    canvas.checkSample({640., 400.}, QColor{255, 0, 0, 64});
    const auto halfEdge = canvas.sample({640., 440.});
    CHECK(halfEdge.red() == 255);
    CHECK(halfEdge.green() == 0);
    CHECK(halfEdge.blue() == 0);
    CHECK(halfEdge.alpha() >= 20);
    CHECK(halfEdge.alpha() <= 32);

    stroke["feather"] = 1.;
    load(paint, {stroke});
    canvas.checkSample({640., 360.}, QColor{255, 0, 0, 64});
    const auto edge = canvas.sample({640., 440.});
    CHECK(edge.red() == 255);
    CHECK(edge.green() == 0);
    CHECK(edge.blue() == 0);
    CHECK(edge.alpha() >= 8);
    CHECK(edge.alpha() <= 20);
    CHECK(edge.alpha() < halfEdge.alpha());
    canvas.checkSample({640., 472.}, QColor{0, 0, 0, 0});
    canvas.checkPixels(
        {{{640., 360.}, QColor{64, 0, 0}},
         {{640., 440.}, QColor{edge.alpha(), 0, 0}},
         {{640., 472.}, Qt::black}});
  });
}

TEST_CASE(
    "Canvas2D feathered previews do not accumulate across samples or repaints",
    "[integration][js][gfx][painting][gui]")
{
  withCanvasApp([] {
    Canvas canvas;
    auto& paint = *canvas.item;
    REQUIRE(paint.setProperty("fillColor", QColor{Qt::transparent}));
    auto config = style("brush", "#ffffff", 200., 0.5);
    config["feather"] = 1.;
    invoke(paint, "beginStroke", config, 320., 360.);
    canvas.checkSample({320., 360.}, QColor{255, 255, 255, 128});
    const auto dotEdge = canvas.sample({320., 440.});
    REQUIRE(dotEdge.alpha() >= 16);
    REQUIRE(dotEdge.alpha() <= 36);
    invoke(paint, "extendStroke", 960., 360.);
    canvas.checkSample({640., 360.}, QColor{255, 255, 255, 128});
    const auto edge = canvas.sample({640., 440.});
    CHECK(std::abs(edge.alpha() - dotEdge.alpha()) <= 2);

    // Retrace the same ink with many input samples, including reversals and joins.
    // This catches per-dab compositing and non-stenciled overlapping strokes.
    for(double x = 920.; x >= 320.; x -= 40.)
      invoke(paint, "extendStroke", x, 360.);
    invoke(paint, "extendStroke", 960., 360.);
    for(int frame = 0; frame < 3; ++frame)
    {
      canvas.frames(1);
      canvas.checkSample({640., 360.}, QColor{255, 255, 255, 128});
      canvas.checkSample({640., 440.}, edge);
    }
    invoke(paint, "cancelStroke");
    canvas.checkPixels({{{640., 360.}, Qt::black}, {{640., 440.}, Qt::black}});
    canvas.checkSample({640., 360.}, QColor{0, 0, 0, 0});

    invoke(paint, "beginStroke", config, 320., 360.);
    invoke(paint, "extendStroke", 640., 360.);
    invoke(paint, "extendStroke", 960., 360.);
    const auto committed = invoke(paint, "endStroke").toMap();
    canvas.checkSample({640., 440.}, edge);
    load(paint, {});
    canvas.checkPixels({{{640., 360.}, Qt::black}});
    load(paint, {committed});
    canvas.checkSample({640., 360.}, QColor{255, 255, 255, 128});
    canvas.checkSample({640., 440.}, edge);
    load(paint, {committed});
    canvas.resize({320, 180});
    canvas.frames();
    canvas.checkSample({640., 440.}, edge);
    canvas.checkSample({640., 472.}, QColor{0, 0, 0, 0});
  });
}

TEST_CASE(
    "Canvas2D feather does not soften shapes or erasure",
    "[integration][js][gfx][painting][gui]")
{
  withCanvasApp([] {
    Canvas canvas;
    auto& paint = *canvas.item;
    auto line = command(style("line", "#ffffff", 200.), {{320., 360.}, {960., 360.}});
    line["feather"] = 1.;
    auto eraser = command(style("eraser", "#ffffff", 100.), {{640., 360.}});
    eraser["feather"] = 1.;
    load(paint, {line, eraser});
    canvas.checkPixels(
        {{{640., 400.}, Qt::black},
         {{640., 440.}, Qt::white},
         {{640., 472.}, Qt::black}});
  });
}

TEST_CASE(
    "Canvas2D history replacement and clear remove obsolete ink",
    "[integration][js][gfx][painting][gui]")
{
  withCanvasApp([] {
    Canvas canvas;
    auto& paint = *canvas.item;
    const auto red = command(
        style("rectangle", "#ff0000", 40., 1., true), {{80., 80.}, {600., 640.}});
    const auto blue = command(
        style("rectangle", "#0000ff", 40., 1., true), {{680., 80.}, {1200., 640.}});
    load(paint, {red, blue});
    canvas.checkPixels({{{320., 360.}, Qt::red}, {{960., 360.}, Qt::blue}});
    load(paint, {red});
    canvas.checkPixels({{{320., 360.}, Qt::red}, {{960., 360.}, Qt::black}});
    auto green = blue;
    green["color"] = "#00ff00";
    load(paint, {red, green});
    canvas.checkPixels({{{320., 360.}, Qt::red}, {{960., 360.}, Qt::green}});
    // Clear is a document command; later strokes survive, and undo restores earlier ink.
    const QVariantMap clear{{"tool", "clear"}};
    load(paint, {red, clear, blue});
    canvas.checkPixels({{{320., 360.}, Qt::black}, {{960., 360.}, Qt::blue}});
    load(paint, {red});
    canvas.checkPixels({{{320., 360.}, Qt::red}, {{960., 360.}, Qt::black}});
    invoke(paint, "beginStroke", style("brush", "#ffffff", 200.), 960., 360.);
    canvas.checkPixels({{{960., 360.}, Qt::white}});
    load(paint, {red});
    canvas.checkPixels({{{320., 360.}, Qt::red}, {{960., 360.}, Qt::black}});
    load(paint, {});
    canvas.checkPixels({{{320., 360.}, Qt::black}, {{960., 360.}, Qt::black}});
  });
}

TEST_CASE(
    "Canvas2D erasure preserves the background and eyedropper straight alpha",
    "[integration][js][gfx][painting][gui]")
{
  withCanvasApp([] {
    Canvas canvas;
    auto& paint = *canvas.item;
    const auto white = command(
        style("rectangle", "#ffffff", 40., 1., true), {{160., 90.}, {1120., 630.}});
    const auto eraser = command(style("eraser", "#ffffff", 200., 0.5), {{640., 360.}});
    load(paint, {white, eraser});
    canvas.checkPixels({{{640., 360.}, QColor{128, 128, 128}}});
    canvas.checkSample({640., 360.}, QColor{128, 128, 128});

    const QColor background{32, 64, 96};
    REQUIRE(paint.setProperty("fillColor", background));
    canvas.checkPixels(
        {{{640., 360.}, QColor{144, 160, 176}}, {{40., 40.}, background}});
    canvas.checkSample({640., 360.}, QColor{144, 160, 176});
    invoke(paint, "beginStroke", style("eraser", "#ffffff", 200.), 640., 360.);
    canvas.checkPixels(
        {{{640., 360.}, background},
         {{320., 360.}, Qt::white},
         {{40., 40.}, background}});
    invoke(paint, "cancelStroke");
    canvas.checkPixels({{{640., 360.}, QColor{144, 160, 176}}});
    auto fullErase = eraser;
    fullErase["opacity"] = 1.;
    load(paint, {white, fullErase});
    canvas.checkPixels({{{640., 360.}, background}, {{40., 40.}, background}});
    canvas.checkSample({640., 360.}, background);

    // A transparent item's readback must retain alpha and unpremultiply once,
    // while its on-screen framebuffer is composited over the black window.
    REQUIRE(paint.setProperty("fillColor", QColor{Qt::transparent}));
    load(paint, {white, eraser});
    canvas.checkPixels({{{640., 360.}, QColor{128, 128, 128}}});
    canvas.checkSample({640., 360.}, QColor{255, 255, 255, 128});
    canvas.checkSample({40., 40.}, QColor{0, 0, 0, 0});
    auto tinted = white;
    tinted["color"] = "#8080ff40";
    load(paint, {tinted});
    canvas.checkPixels({{{640., 360.}, QColor{64, 128, 32}}});
    canvas.checkSample({640., 360.}, QColor{128, 255, 64, 128});
  });
}

TEST_CASE(
    "Canvas2D asynchronous samples cannot outlive their request or document",
    "[integration][js][gfx][painting][gui]")
{
  withCanvasApp([] {
    Canvas canvas;
    auto& paint = *canvas.item;
    const auto red = command(
        style("rectangle", "#ff0000", 40., 1., true), {{80., 80.}, {600., 640.}});
    load(paint, {red});
    canvas.checkPixels({{{320., 360.}, Qt::red}});
    SignalLog sampled{&paint, "testSamples"};

    SECTION("a newer sample supersedes an earlier coordinate")
    {
      invoke(paint, "sampleColor", 320., 360.);
      invoke(paint, "sampleColor", 960., 360.);
      REQUIRE(eventually([&] { return !sampled.empty(); }));
      canvas.frames();
      REQUIRE(sampled.size() == 1);
      CHECK(near(sampled.front().front().value<QColor>(), Qt::black));
    }
    SECTION("document replacement cancels an outstanding sample")
    {
      invoke(paint, "sampleColor", 320., 360.);
      load(paint, {});
      canvas.frames();
      CHECK(sampled.empty());
      canvas.checkSample({320., 360.}, Qt::black);
    }
    SECTION("a scheduled image grab cannot deliver after document replacement")
    {
      invoke(paint, "sampleColor", 320., 360.);
      // Render the request, then deliver queued GUI work without processing the
      // next frame or the grab-completion event. This allows the asynchronous
      // grab to start before invalidation, unlike canceling a still-pending sample.
      REQUIRE_FALSE(canvas.window.grabWindow().isNull());
      QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
      REQUIRE(sampled.empty());
      load(paint, {});
      canvas.frames();
      CHECK(sampled.empty());
      canvas.checkSample({320., 360.}, Qt::black);
    }
    SECTION("changing background cancels an outstanding sample")
    {
      invoke(paint, "sampleColor", 960., 360.);
      REQUIRE(paint.setProperty("fillColor", QColor{Qt::blue}));
      canvas.frames();
      CHECK(sampled.empty());
      canvas.checkSample({960., 360.}, Qt::blue);
    }
    SECTION("a new gesture cancels a sample of the old canvas")
    {
      invoke(paint, "sampleColor", 320., 360.);
      invoke(paint, "beginStroke", style("brush", "#00ff00", 200.), 320., 360.);
      canvas.frames();
      CHECK(sampled.empty());
      canvas.checkSample({320., 360.}, Qt::green);
    }
  });
}

TEST_CASE(
    "PaintView local and remote gestures use fixed document coordinates after host "
    "resize",
    "[integration][js][gfx][painting][gui]")
{
  withCanvasApp([] {
    Canvas canvas{true};
    auto& view = *canvas.item;
    const QColor background{32, 48, 64};
    REQUIRE(view.setProperty("backgroundColor", background));
    canvas.resize({800, 360});
    canvas.frames();
    SignalLog committed{&view, "testCommits"};
    SignalLog gestures{&view, "testGestures"};
    REQUIRE(view.setProperty("config", style("rectangle", "#ff0000", 40., 1., true)));
    canvas.mouse(QEvent::MouseButtonPress, {320., 180.});
    canvas.mouse(QEvent::MouseMove, {960., 540.});
    canvas.checkPixels({{{640., 360.}, Qt::red}});
    CHECK(committed.empty());
    canvas.mouse(QEvent::MouseButtonRelease, {960., 540.});
    REQUIRE(committed.size() == 1);
    const auto local = plain(committed.front().front()).toMap();
    CHECK(
        local.value("points").toList()
        == QVariantList{QVariantList{320., 180.}, QVariantList{960., 540.}});
    REQUIRE(gestures.size() >= 2);
    const auto begin = plain(gestures.front().front()).toMap();
    const auto end = plain(gestures.back().front()).toMap();
    CHECK(begin.value("action").toString() == "begin");
    CHECK(begin.value("x").toDouble() == 320.);
    CHECK(begin.value("y").toDouble() == 180.);
    CHECK(end.value("action").toString() == "end");
    CHECK(end.value("x").toDouble() == 960.);
    CHECK(end.value("y").toDouble() == 540.);

    canvas.resize({400, 400});
    canvas.checkPixels(
        {{{640., 360.}, Qt::red}, {{80., 360.}, background}, {{640., 80.}, background}});
    const auto frame = canvas.window.grabWindow();
    REQUIRE_FALSE(frame.isNull());
    CHECK(near(canvas.pixel(frame, {200., 20.}), Qt::black));
    REQUIRE(view.setProperty("config", style("brush", "#00ff00", 60.)));
    canvas.mouse(QEvent::MouseButtonPress, {320., 280.});
    canvas.mouse(QEvent::MouseButtonRelease, {320., 280.});
    REQUIRE(committed.size() == 2);
    const auto resizedStroke = plain(committed.back().front()).toMap();
    const auto resizedPoints = resizedStroke.value("points").toList();
    REQUIRE(resizedPoints.size() == 1);
    const auto coordinates = resizedPoints.front().toList();
    REQUIRE(coordinates.size() == 2);
    CHECK(std::abs(coordinates[0].toDouble() - 320.) < 1e-8);
    CHECK(std::abs(coordinates[1].toDouble() - 280.) < 1e-8);
    canvas.checkPixels({{{320., 280.}, Qt::green}, {{640., 360.}, Qt::red}});
    invoke(view, "load", QVariantMap{{"strokes", QVariantList{local}}});
    committed.clear();
    gestures.clear();
    const QVariantMap remoteBegin{
        {"action", "begin"},
        {"config", style("line", "#0000ff", 60.)},
        {"x", 160.},
        {"y", 90.}};
    invoke(view, "receive", remoteBegin);
    invoke(view, "receive", QVariantMap{{"action", "move"}, {"x", 1120.}, {"y", 90.}});
    canvas.checkPixels({{{640., 90.}, Qt::blue}, {{640., 360.}, Qt::red}});
    invoke(view, "receive", QVariantMap{{"action", "cancel"}});
    canvas.checkPixels({{{640., 90.}, background}, {{640., 360.}, Qt::red}});
    invoke(view, "receive", remoteBegin);
    invoke(view, "receive", QVariantMap{{"action", "end"}, {"x", 1120.}, {"y", 90.}});
    canvas.checkPixels({{{640., 90.}, Qt::blue}, {{640., 360.}, Qt::red}});
    CHECK(committed.empty());
    CHECK(
        gestures.empty()); // Remote replay must not echo gestures or duplicate commits.
    invoke(view, "load", QVariantMap{{"strokes", QVariantList{local}}});
    canvas.checkPixels({{{640., 90.}, background}, {{640., 360.}, Qt::red}});

    SignalLog sampled{&view, "testSamples"};
    REQUIRE(view.setProperty("config", style("eyedropper")));
    canvas.mouse(QEvent::MouseButtonPress, {640., 360.});
    canvas.mouse(QEvent::MouseButtonRelease, {640., 360.});
    REQUIRE(eventually([&] { return !sampled.empty(); }));
    REQUIRE(sampled.size() == 1);
    CHECK(near(sampled.front().front().value<QColor>(), Qt::red));
    CHECK(committed.empty());
    CHECK(gestures.empty());

    sampled.clear();
    canvas.mouse(QEvent::MouseButtonPress, {640., 360.});
    REQUIRE(view.setProperty("config", style("brush")));
    canvas.mouse(QEvent::MouseButtonRelease, {640., 360.});
    canvas.frames();
    CHECK(sampled.empty()); // Switching tools cannot apply an old eyedropper result.
    CHECK(committed.empty());

    // The display-only transparency grid must never become picked paint.
    REQUIRE(view.setProperty("checkerboard", true));
    REQUIRE(view.setProperty("backgroundColor", QColor{Qt::transparent}));
    invoke(view, "load", QVariantMap{{"strokes", QVariantList{}}});
    canvas.frames();
    REQUIRE(view.setProperty("config", style("eyedropper")));
    canvas.mouse(QEvent::MouseButtonPress, {320., 360.});
    canvas.mouse(QEvent::MouseButtonRelease, {320., 360.});
    REQUIRE(eventually([&] { return !sampled.empty(); }));
    REQUIRE(sampled.size() == 1);
    CHECK(near(sampled.front().front().value<QColor>(), Qt::transparent));
  });
}

TEST_CASE(
    "Painting releases text editing before document undo shortcuts",
    "[integration][js][gfx][painting][gui]")
{
  withCanvasApp([] {
    Canvas canvas{true};
    QQmlComponent component{&canvas.engine};
    component.setData(
        "import QtQuick\nTextInput { width: 60; height: 24; text: \"5\" }", QUrl{});
    REQUIRE(component.isReady());
    std::unique_ptr<QQuickItem> field{qobject_cast<QQuickItem*>(component.create())};
    REQUIRE(field != nullptr);
    field->setParentItem(canvas.window.contentItem());
    field->forceActiveFocus();
    const auto key = [&](int code, Qt::KeyboardModifiers modifiers, QString text = {}) {
      QKeyEvent press{QEvent::KeyPress, code, modifiers, text};
      QCoreApplication::sendEvent(&canvas.window, &press);
      QKeyEvent release{QEvent::KeyRelease, code, modifiers, text};
      QCoreApplication::sendEvent(&canvas.window, &release);
    };
    key(Qt::Key_0, Qt::NoModifier, "0");
    const auto edited = field->property("text").toString();
    REQUIRE(edited != "5");
    canvas.mouse(QEvent::MouseButtonPress, {640., 360.});
    canvas.mouse(QEvent::MouseButtonRelease, {640., 360.});
    key(Qt::Key_Z, Qt::ControlModifier);
    // Without canvas focus, TextInput consumes Ctrl+Z and undoes the old edit
    // instead of allowing score's document shortcut to handle the keystroke.
    CHECK(field->property("text").toString() == edited);
  });
}

TEST_CASE(
    "Canvas2D restores documents loaded while hidden",
    "[integration][js][gfx][painting][gui]")
{
  withCanvasApp([] {
    Canvas canvas;
    auto& paint = *canvas.item;
    paint.setVisible(false);
    load(
        paint, {command(
                   style("rectangle", "#ff0000", 40., 1., true),
                   {{160., 90.}, {1120., 630.}})});
    canvas.frames();
    paint.setVisible(true);
    canvas.checkPixels({{{640., 360.}, Qt::red}});
    canvas.checkSample({640., 360.}, Qt::red);
  });
}
