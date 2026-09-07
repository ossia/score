// Exercise the real Javascript execution factory and GPU renderer, with an
// offscreen RHI output as in ShaderSweep. No transport clock runs: ticks are
// stepped explicitly so a late renderer cannot accidentally receive a fresh
// control update instead of restoring the retained snapshot.
#include <Process/Dataflow/Port.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Settings/Model.hpp>
#include <JS/JSProcessModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <ossia/audio/audio_tick.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>

#include <QImage>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

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

void deliverUiMessages()
{
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
}

Process::ControlInlet& control(JS::ProcessModel& process, const QString& name)
{
  for(auto* inlet : process.inlets())
    if(inlet->name() == name)
      if(auto* result = qobject_cast<Process::ControlInlet*>(inlet))
        return *result;
  FAIL("Javascript control was not created: " << name.toStdString());
  throw std::runtime_error{"missing control"};
}

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

// Own the factory-created component and its scheduler. Never drain the document's
// live SPSC audio queue from a second consumer.
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

void checkFrame(const QRhiReadbackResult& readback, const QColor& expected)
{
  REQUIRE(readback.pixelSize == QSize{64, 64});
  REQUIRE(readback.data.size() == 64 * 64 * 4);
  const QImage image{
      reinterpret_cast<const unsigned char*>(readback.data.constData()), 64, 64, 64 * 4,
      QImage::Format_RGBA8888};
  // The rendered colour encodes both the retained control and impulse count;
  // no font, OCR or cross-library signal-pointer comparison is involved.
  for(const QPoint point : {QPoint{8, 8}, QPoint{32, 32}, QPoint{55, 55}})
  {
    const auto pixel = image.pixelColor(point);
    CAPTURE(pixel.red(), pixel.green(), pixel.blue(), pixel.alpha());
    CHECK(pixel.red() == expected.red());
    CHECK(pixel.green() == expected.green());
    CHECK(pixel.blue() == expected.blue());
    CHECK(pixel.alpha() == 255);
  }
}
}

TEST_CASE(
    "Javascript GPU renderers restore controls and deliver their snapshot once",
    "[integration][js][gpu][gui]")
{
#if !defined(SCORE_HAS_GPU_JS) || defined(QT_NO_OPENGL)
  SKIP("Javascript GPU execution and Qt OpenGL support are required");
#else
  if(qEnvironmentVariable("QT_QUICK_BACKEND") == QStringLiteral("software")
     || qEnvironmentVariable("QSG_RHI_BACKEND") == QStringLiteral("software"))
    SKIP("The software Qt Quick renderer cannot exercise Javascript GPU rendering");

  bool lateRenderer = false;
  SECTION("renderer created before the first execution tick") { }
  SECTION("renderer created after controls ran while playback is stopped")
  {
    lateRenderer = true;
  }

  QTemporaryDir settings;
  REQUIRE(settings.isValid());
  Environment config{"XDG_CONFIG_HOME", settings.path().toUtf8()};
  Environment backend{"QSG_RHI_BACKEND", "opengl"};

  Environment renderLoop{"QSG_RENDER_LOOP", "basic"};
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    // A capability failure is a skip; a blank/missing frame after this probe is
    // a regression, not an excuse to silently pass a software renderer.
    QOpenGLContext probe;
    if(!probe.create())
      SKIP("A working OpenGL context is required (use Xvfb with GLX or a display)");
    QOffscreenSurface surface;
    surface.setFormat(probe.format());
    surface.create();
    if(!surface.isValid() || !probe.makeCurrent(&surface))
      SKIP("The display cannot make an offscreen OpenGL surface current");
    probe.doneCurrent();

    ctx.settings<Gfx::Settings::Model>().setGraphicsApi(QStringLiteral("OpenGL"));
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
import Score
Script {
  id: root
  property int deliveries: 0
  Impulse {
    objectName: "Pulse"
    onImpulse: {
      root.deliveries++;
    }
  }
  FloatSlider { id: level; objectName: "Level"; init: 0.25 }
  ValueOutlet { objectName: "Events" }
  TextureOutlet {
    objectName: "Output"
    item: Rectangle {
      anchors.fill: parent
      color: root.deliveries === 1 && level.value === 0.75 ? "#00ff00"
           : root.deliveries === 2 && level.value === 0.5 ? "#ff0000" : "#ff00ff"
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
    auto& pulse = control(*process, QStringLiteral("Pulse"));
    auto& level = control(*process, QStringLiteral("Level"));
    pulse.setValue(1);
    if(!lateRenderer)
      level.setValue(0.75f);

    auto& execution = doc->context().plugin<Execution::DocumentPlugin>();
    auto& graphics = doc->context().plugin<Gfx::DocumentPlugin>();
    REQUIRE_FALSE(execution.isPlaying());
    auto context
        = std::make_shared<Execution::DocumentPlugin::ContextData>(doc->context());
    context->context.alias = context;
    context->execGraph = ossia::make_graph(ossia::graph_setup_options{});
    context->execState = std::make_shared<ossia::execution_state>();
    auto& state = *context->execState;
    state.sampleRate = 48000;
    state.bufferSize = 64;
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
    REQUIRE(node->id != score::gfx::invalid_node_index);

    ossia::token_request token;
    token.date = ossia::time_value{940800};
    token.parent_duration = ossia::time_value{705600000};
    token.speed = 1.;
    token.tempo = 120.;
    token.signature = ossia::time_signature{4, 4};
    token.length_sample = 64;
    auto tick = [&] {
      execute(context->m_execQueue, [&] {
        node->run(token, ossia::exec_state_facade{&state});
      });
      graphics.context.updateGraph();
    };

    if(lateRenderer)
    {
      tick();
      level.setValue(0.75f);
      tick();
      // The impulse is retained even though the last tick only changed Level.
      // No tick will be executed between creating the output and first paint.
    }
    deliverUiMessages();
    REQUIRE_FALSE(execution.isPlaying());

    auto output = std::make_unique<score::gfx::BackgroundNode>();
    auto readback = std::make_shared<QRhiReadbackResult>();
    output->shared_readback = readback;
    output->setRenderSize(QSize{64, 64});
    const int outputId = graphics.context.register_node(std::move(output));
    graphics.exec.setEdge(
        {node->id, 1}, {outputId, 0}, Process::CableType::ImmediateGlutton);
    graphics.exec.endTick(ossia::audio_tick_state{});
    graphics.context.renderFrames(4);
    deliverUiMessages();
    checkFrame(*readback, QColor{Qt::green});
    REQUIRE_FALSE(execution.isPlaying());

    // More paints are not more deliveries of the retained impulse.
    graphics.context.renderFrames(4);
    deliverUiMessages();
    checkFrame(*readback, QColor{Qt::green});

    // A new control change is one new delivery, not another snapshot replay.
    level.setValue(0.5f);
    pulse.setValue(2);
    tick();
    graphics.context.renderFrames(4);
    deliverUiMessages();
    checkFrame(*readback, QColor{Qt::red});
    graphics.context.renderFrames(4);
    deliverUiMessages();
    checkFrame(*readback, QColor{Qt::red});
    CHECK_FALSE(execution.isPlaying());
  });
#endif
}
