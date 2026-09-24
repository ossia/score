// A Javascript TextureOutlet showing an offscreen Qt Quick 3D View3D keeps the
// Vulkan device alive when another node has already drawn earlier in the frame.
//
// The graph is Images (magenta) -> mix.a, View3D (green) -> mix.b, mix ->
// readback sink, with 4x MSAA, so the mixer's first input records a draw pass
// before the Javascript node renders its Qt Quick scene. On Vulkan that used to
// lose the device on the first frame and every frame stayed blank; the mixed
// frame must now carry both inputs. Without MSAA it did not reproduce.
#include <Process/Dataflow/Port.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/ImageNode.hpp>
#include <Gfx/Settings/Model.hpp>
#include <JS/JSProcessModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/gfx/Vulkan.hpp>

#include <ossia/audio/audio_tick.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>

#include <QImage>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Gfx.hpp>

#include <memory>
#include <thread>

namespace
{
struct Environment
{
  QByteArray key;
  QByteArray old;
  bool existed;

  Environment(const char* name, const QByteArray& value)
      : key{name}
      , old{qgetenv(name)}
      , existed{qEnvironmentVariableIsSet(name)}
  {
    qputenv(key.constData(), value);
  }
  ~Environment()
  {
    if(existed)
      qputenv(key.constData(), old);
    else
      qunsetenv(key.constData());
  }
};

template <typename Tick>
void execute(Execution::ExecutionCommandQueue& queue, Tick&& tick)
{
  std::thread worker{[&] {
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    Execution::ExecutionCommand command;
    while(queue.try_dequeue(command))
      command();
    tick();
  }};
  worker.join();
}

struct ComponentOwner
{
  Execution::ExecutionCommandQueue& queue;
  std::shared_ptr<Execution::ProcessComponent> component;

  ~ComponentOwner()
  {
    if(component)
    {
      component->cleanup();
      execute(queue, [] { });
      component.reset();
    }
  }
};

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
  Environment config{"XDG_CONFIG_HOME", settings.path().toUtf8()};
  Environment backend{"QSG_RHI_BACKEND", "vulkan"};
  Environment renderLoop{"QSG_RENDER_LOOP", "basic"};

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
    const auto program = process->setProgram(
        JS::QmlSource{
            QStringLiteral(R"QML(
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
)QML"),
            {}});
    REQUIRE(program.valid);
    process->programChanged();
    process->inletsChanged();
    process->outletsChanged();
    REQUIRE(process->isGpu());

    auto& graphics = doc->context().plugin<Gfx::DocumentPlugin>();
    auto context
        = std::make_shared<Execution::DocumentPlugin::ContextData>(doc->context());
    context->context.alias = context;
    context->execGraph = ossia::make_graph(ossia::graph_setup_options{});
    context->execState = std::make_shared<ossia::execution_state>();
    context->execState->sampleRate = 48000;
    context->execState->bufferSize = 64;
    auto* executorFactory
        = ctx.interfaces<Execution::ProcessComponentFactoryList>().factory(*process);
    REQUIRE(executorFactory != nullptr);
    ComponentOwner owner{
        context->m_execQueue,
        executorFactory->make(*process, context->context, nullptr)};
    REQUIRE(owner.component != nullptr);
    execute(context->m_execQueue, [] { });
    auto* node = dynamic_cast<Gfx::gfx_exec_node*>(owner.component->node.get());
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
