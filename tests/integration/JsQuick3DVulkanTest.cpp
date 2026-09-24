// A Javascript TextureOutlet showing an offscreen Qt Quick 3D View3D keeps the
// Vulkan device alive when another node has already drawn earlier in the frame.
//
// The graph is Images (magenta) -> mix.a, View3D (green) -> mix.b, mix ->
// readback sink, with 4x MSAA, so the mixer's first input records a draw pass
// before the Javascript node renders its Qt Quick scene. The mixed frame must
// carry both inputs: a lost Vulkan device leaves every frame blank. MSAA is part
// of the configuration under test.
#include <Process/Dataflow/Port.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/ImageNode.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/gfx/Vulkan.hpp>

#include <ossia/audio/audio_tick.hpp>

#include <QImage>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/JsGpu.hpp>
#include <score_test/Gfx.hpp>

#include <memory>

namespace
{
using score::test::scoped_env;

QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}
}

TEST_CASE(
    "An offscreen View3D after an earlier draw keeps the Vulkan device",
    "[integration][js][gpu][vulkan][gui]")
{
#if !defined(SCORE_HAS_GPU_JS) || !QT_HAS_VULKAN
  SKIP("Javascript GPU execution and Vulkan support are required");
#else
  QTemporaryDir settings;
  REQUIRE(settings.isValid());
  scoped_env config{"XDG_CONFIG_HOME", settings.path().toUtf8()};
  scoped_env backend{"QSG_RHI_BACKEND", "vulkan"};
  scoped_env renderLoop{"QSG_RENDER_LOOP", "basic"};

  const QSize size{1920, 1080};
  bool skipped = false;
  QImage frame;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    ctx.settings<Gfx::Settings::Model>().setGraphicsApi(QStringLiteral("Vulkan"));
    ctx.settings<Gfx::Settings::Model>().setSamples(4);
    if(!score::gfx::staticVulkanInstance())
    {
      skipped = true;
      return;
    }
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto* process = &score::test::add_js_process(ctx, *doc, QStringLiteral(R"QML(
import QtQuick
import QtQuick3D
import Score
Script {
  TextureOutlet {
    objectName: "Output"
    item: View3D {
      anchors.fill: parent
      environment: SceneEnvironment {
        clearColor: "#00ff00"
        backgroundMode: SceneEnvironment.Color
      }
      PerspectiveCamera { }
    }
  }
}
)QML"));

    auto& graphics = doc->context().plugin<Gfx::DocumentPlugin>();
    score::test::js_gpu_executor exec{ctx, *doc, *process};
    auto* node = exec.node;
    REQUIRE(node != nullptr);

    QImage magenta{64, 64, QImage::Format_ARGB32};
    magenta.fill(qRgb(255, 0, 255));
    const QString png = settings.path() + QStringLiteral("/magenta.png");
    REQUIRE(magenta.save(png, "PNG"));
    auto images = std::make_unique<score::gfx::ImagesNode>(doc->context());
    {
      score::gfx::Message m;
      m.input.resize(8);
      m.input[0] = ossia::value{0};
      m.input[1] = ossia::value{1.f};
      m.input[2] = ossia::value{ossia::vec2f{0.f, 0.f}};
      m.input[3] = ossia::value{1.f};
      m.input[4] = ossia::value{1.f};
      m.input[5] = ossia::value{std::vector<ossia::value>{png.toStdString()}};
      m.input[6] = ossia::value{0};
      m.input[7] = ossia::value{(int)score::gfx::ScaleMode::Stretch};
      static_cast<score::gfx::Node&>(*images).process(std::move(m));
    }
    auto mix = score::test::gfx::make_isf_node(corpus("isf-mix-two.fs"));
    INFO(mix.error);
    REQUIRE(mix.node);
    const int imagesId = graphics.context.register_node(std::move(images));
    const int mixId = graphics.context.register_node(std::move(mix.node));

    auto output = std::make_unique<score::gfx::BackgroundNode>();
    auto readback = std::make_shared<QRhiReadbackResult>();
    output->shared_readback = readback;
    output->setRenderSize(size);
    const int outputId = graphics.context.register_node(std::move(output));

    graphics.exec.setEdge({imagesId, 0}, {mixId, 0}, Process::CableType::ImmediateGlutton);
    graphics.exec.setEdge({node->id, 0}, {mixId, 1}, Process::CableType::ImmediateGlutton);
    graphics.exec.setEdge({mixId, 0}, {outputId, 0}, Process::CableType::ImmediateGlutton);
    graphics.exec.endTick(ossia::audio_tick_state{});
    graphics.context.renderFrames(8);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);

    if(readback->pixelSize == size
       && readback->data.size() == size.width() * size.height() * 4)
      frame = QImage{
          reinterpret_cast<const unsigned char*>(readback->data.constData()),
          size.width(), size.height(), size.width() * 4, QImage::Format_RGBA8888}
                  .copy();
  });
  if(skipped)
    SKIP("No Vulkan instance");

  REQUIRE_FALSE(frame.isNull());
  for(const QPoint point :
      {QPoint{100, 100}, QPoint{size.width() / 2, size.height() / 2},
       QPoint{size.width() - 100, size.height() - 100}})
  {
    const auto pixel = frame.pixelColor(point);
    CAPTURE(point.x(), point.y(), pixel.red(), pixel.green(), pixel.blue());
    CHECK(pixel.red() > 64);
    CHECK(pixel.green() > 64);
    CHECK(pixel.blue() > 64);
  }
#endif
}
