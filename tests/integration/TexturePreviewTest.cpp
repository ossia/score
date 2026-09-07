// Exercise TextureSource through a Javascript process's real ScriptUI. A value
// outlet deliberately precedes the image: node registration alone is not enough
// to display the selected texture, and a stopped node must never be reused.
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <Gfx/TexturePort.hpp>
#include <JS/JSProcessModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QOffscreenSurface>
#include <QOpenGLContext>
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
bool eventually(Predicate&& predicate)
{
  QElapsedTimer timer;
  timer.start();
  do
  {
    spin(10);
    if(predicate())
      return true;
  } while(timer.elapsed() < 10000);
  return false;
}

constexpr auto previewUi = R"QML(import QtQuick
import Score
import Score.UI
ScriptUI {
  width: 480
  height: 270
  TextureSource {
    objectName: "selectedPreview"
    anchors.fill: parent
    process: "texture-preview-source"
    port: "Output"
  }
}
)QML";

QString executionScript(const QString& left, const QString& right)
{
  return QString::fromLatin1(R"QML(import QtQuick
import Score
Script {
  ValueOutlet { objectName: "Events" }
  TextureOutlet {
    objectName: "Output"
    item: Item {
      width: 480
      height: 270
      Rectangle {
        anchors.left: parent.left
        width: parent.width / 2
        height: parent.height
        color: "%1"
      }
      Rectangle {
        anchors.right: parent.right
        width: parent.width / 2
        height: parent.height
        color: "%2"
      }
    }
  }
  tick: function(token, state) { }
}
)QML")
      .arg(left, right);
}

void setProgram(JS::ProcessModel& process, const QString& left, const QString& right)
{
  const auto result = process.setProgram(
      JS::QmlSource{executionScript(left, right), QString::fromLatin1(previewUi)});
  REQUIRE(result.valid);
  process.programChanged();
  process.inletsChanged();
  process.outletsChanged();
}

Gfx::TextureOutlet& textureOutlet(JS::ProcessModel& process)
{
  REQUIRE(process.outlets().size() == 2);
  REQUIRE(process.outlets()[0]->name() == QStringLiteral("Events"));
  auto* output = qobject_cast<Gfx::TextureOutlet*>(process.outlets()[1]);
  REQUIRE(output != nullptr);
  REQUIRE(output->name() == QStringLiteral("Output"));
  return *output;
}

bool near(const QColor& actual, const QColor& expected)
{
  constexpr int tolerance = 24;
  return std::abs(actual.red() - expected.red()) <= tolerance
         && std::abs(actual.green() - expected.green()) <= tolerance
         && std::abs(actual.blue() - expected.blue()) <= tolerance;
}

bool hasHalves(const QImage& image, const QColor& left, const QColor& right)
{
  if(image.isNull() || image.width() < 16 || image.height() < 16)
    return false;
  for(int y : {1, 2, 3})
  {
    for(int x : {1, 2, 3})
    {
      if(!near(image.pixelColor(image.width() * x / 8, image.height() * y / 4), left)
         || !near(
             image.pixelColor(image.width() * (x + 4) / 8, image.height() * y / 4),
             right))
        return false;
    }
  }
  return true;
}

void requirePreview(QQuickWindow& window, const QColor& left, const QColor& right)
{
  QImage image;
  const bool matched = eventually([&] {
    window.update();
    image = window.grabWindow();
    return hasHalves(image, left, right);
  });
  INFO("Preview readback: " << image.width() << " x " << image.height());
  if(!image.isNull())
  {
    INFO(
        "Left: "
        << image.pixelColor(image.width() / 4, image.height() / 2).name().toStdString());
    INFO(
        "Right: " << image.pixelColor(image.width() * 3 / 4, image.height() / 2)
                         .name()
                         .toStdString());
    REQUIRE(matched);
  }
  else
  {
    FAIL("A usable OpenGL context exists, but the actual preview returned no image");
  }
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
}

TEST_CASE(
    "Javascript texture previews select an endpoint and forget stopped nodes",
    "[integration][js][gfx][preview][gui]")
{
#if !defined(SCORE_HAS_GPU_JS)
  SKIP("This build has no native Javascript GPU support (SCORE_HAS_GPU_JS)");
#else
  if(qgetenv("QT_QUICK_BACKEND") == "software")
    SKIP(
        "TextureSource requires QRhi; unset QT_QUICK_BACKEND=software and use Xvfb with "
        "GL");

  QTemporaryDir settings;
  REQUIRE(settings.isValid());
  EnvironmentGuard config{"XDG_CONFIG_HOME", settings.path().toUtf8()};
  EnvironmentGuard backend{"QSG_RHI_BACKEND", "opengl"};
  // Match src/app/main.cpp: graph previews run on score's GUI render loop.
  EnvironmentGuard renderLoop{"QSG_RENDER_LOOP", "basic"};
  EnvironmentGuard audio{"SCORE_AUDIO_BACKEND", "dummy"};

  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    // Probe the capability independently. Blank routed output below is a failure,
    // never a reason to skip a regression in node registration or outlet choice.
    QOpenGLContext gl;
    if(!gl.create())
      SKIP("No OpenGL context is available; run under a GL-capable display or Xvfb");
    QOffscreenSurface surface;
    surface.setFormat(gl.format());
    surface.create();
    if(!surface.isValid() || !gl.makeCurrent(&surface))
      SKIP("The display cannot make an OpenGL offscreen surface current");
    gl.doneCurrent();

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    const auto key = UuidKey<Process::ProcessModel>::fromString(
        QStringLiteral("846a5de5-47f9-46c5-a898-013cb20951d0"));
    auto* factory = ctx.interfaces<Process::ProcessFactoryList>().get(key);
    REQUIRE(factory != nullptr);
    CommandDispatcher<> dispatcher{doc->context().commandStack};
    dispatcher.submit<Scenario::Command::AddOnlyProcessToInterval>(
        interval, factory->concreteKey(), factory->customConstructionData(), QPointF{});
    JS::ProcessModel* process{};
    for(auto& candidate : interval.processes)
      if(candidate.concreteKey() == key)
        process = qobject_cast<JS::ProcessModel*>(&candidate);
    REQUIRE(process != nullptr);
    process->metadata().setLabel(QStringLiteral("texture-preview-source"));
    setProgram(*process, "#ff0000", "#0000ff");
    REQUIRE(process->isGpu());
    CHECK(textureOutlet(*process).graphicsPort().node == -1);

    auto* execution = doc->context().findPlugin<Execution::DocumentPlugin>();
    REQUIRE(execution != nullptr);
    auto& transport = execution->executionController().transport();

    QQuickWindow window;
    window.setColor(Qt::black);
    window.resize(480, 270);
    std::unique_ptr<QQuickItem> ui{process->createItemForUI(doc->context())};
    REQUIRE(ui != nullptr);
    ui->setParentItem(window.contentItem());
    auto* preview = ui->findChild<QQuickItem*>(QStringLiteral("selectedPreview"));
    REQUIRE(preview != nullptr);
    window.show();
    REQUIRE(eventually([&] { return window.isExposed(); }));
    REQUIRE(window.rendererInterface()->graphicsApi() == QSGRendererInterface::OpenGL);
    StopOnExit stop{transport};

    transport.requestPlay();
    REQUIRE(eventually([&] {
      return execution->isPlaying() && process->executing()
             && textureOutlet(*process).graphicsPort().node >= 0;
    }));
    CHECK(textureOutlet(*process).graphicsPort().port == 1);
    requirePreview(window, Qt::red, Qt::blue);

    // Disconnecting must clear the image, and integer selection must mean the
    // model's outlet ordinal, not the ordinal among texture-only outlets.
    REQUIRE(preview->setProperty("port", -1));
    requirePreview(window, Qt::black, Qt::black);
    REQUIRE(preview->setProperty("port", 1));
    requirePreview(window, Qt::red, Qt::blue);
    REQUIRE(preview->setProperty("port", QStringLiteral("Output")));
    requirePreview(window, Qt::red, Qt::blue);

    transport.requestStop();
    REQUIRE(eventually([&] {
      return !execution->isPlaying() && !process->executing()
             && textureOutlet(*process).graphicsPort().node == -1;
    }));
    requirePreview(window, Qt::black, Qt::black);
    spin(200);
    CHECK(textureOutlet(*process).graphicsPort().node == -1);

    // A different image makes a retained old render node observable. The UI
    // remains open across the model's script replacement and transport restart.
    setProgram(*process, "#00ff00", "#ffff00");
    CHECK(textureOutlet(*process).graphicsPort().node == -1);
    transport.requestPlay();
    REQUIRE(eventually([&] {
      return execution->isPlaying() && process->executing()
             && textureOutlet(*process).graphicsPort().node >= 0;
    }));
    CHECK(textureOutlet(*process).graphicsPort().port == 1);
    requirePreview(window, Qt::green, Qt::yellow);

    transport.requestStop();
    REQUIRE(eventually([&] {
      return !execution->isPlaying()
             && textureOutlet(*process).graphicsPort().node == -1;
    }));
    requirePreview(window, Qt::black, Qt::black);

    // Stop before queued graphics-node publication has drained. A callback from
    // the discarded execution component must not resurrect its model endpoint.
    transport.requestPlay();
    transport.requestStop();
    spin(300);
    CHECK_FALSE(execution->isPlaying());
    CHECK(textureOutlet(*process).graphicsPort().node == -1);
    requirePreview(window, Qt::black, Qt::black);
  });
#endif
}
