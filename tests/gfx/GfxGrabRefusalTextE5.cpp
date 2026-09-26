// WindowDevice::grabTo refusals name the entry point and say what to do.
//
// Two refusals: grabTo reached from inside a frame (here a sink's readback
// completion, which QRhi runs inside endOffscreenFrame while renderFrames is
// on the stack), and the device closed while grabTo pumps the event loop (the
// GfxGrabTeardownF2 scenario). Each writes nothing and logs one warning that
// starts with "score.gfx: grabTo" and tells the caller what to change, like
// GfxContext's own re-entry refusals.
//
// Registration: see test_gfx_process_grab_refusal_text_e5.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Gfx.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Settings/Model.hpp>
#include <Gfx/WindowDevice.hpp>

#include <core/document/Document.hpp>

#include <QFile>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

namespace
{
QString corpus(const char* name)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(name);
}

QStringList g_warnings;
QtMessageHandler g_previousHandler{};
void collectWarnings(QtMsgType t, const QMessageLogContext& c, const QString& msg)
{
  if(t == QtWarningMsg)
    g_warnings.push_back(msg);
  if(g_previousHandler)
    g_previousHandler(t, c, msg);
}

int warningsContaining(const QString& a, const QString& b)
{
  int n = 0;
  for(const auto& w : g_warnings)
    if(w.contains(a) && w.contains(b))
      n++;
  return n;
}

constexpr auto kName = "GrabRefusalE5";

Gfx::WindowDevice* makeDevice(const score::DocumentContext& ctx)
{
  Device::DeviceSettings set;
  set.protocol = Gfx::WindowProtocolFactory::static_concreteKey();
  set.name = QString::fromUtf8(kName);
  auto* dev = new Gfx::WindowDevice{set, ctx};
  if(!dev->reconnect())
  {
    delete dev;
    return nullptr;
  }
  return dev;
}

QString apiSettingName(score::gfx::GraphicsApi api)
{
  const Gfx::Settings::GraphicsApis apis{};
  switch(api)
  {
    case score::gfx::Vulkan:
      return apis.Vulkan;
    case score::gfx::Metal:
      return apis.Metal;
    case score::gfx::D3D11:
      return apis.D3D11;
    case score::gfx::D3D12:
      return apis.D3D12;
    default:
      return apis.OpenGL;
  }
}

struct Outcome
{
  std::string skip;
  bool built{};
  bool calledInFrame{};
  int inFrameRefusals{-1};
  int closedRefusals{-1};
  bool written{};
};

Outcome run(score::gfx::GraphicsApi backend)
{
  Outcome out;
  qputenv("SCORE_FORCE_OFFSCREEN_WINDOW", kName);
  QTemporaryDir dir;
  const QString png = dir.filePath("grab.png");

  score::test::run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto& settings = app.settings<Gfx::Settings::Model>();
    settings.setVSync(false);
    settings.setRate(60.);
    settings.setGraphicsApi(apiSettingName(backend));

    std::string probed;
    if(!score::test::gfx::probe_api(backend, probed))
    {
      out.skip = std::string("RHI backend '") + score::test::gfx::backend_name(backend)
                 + "' unavailable";
      return;
    }

    score::Document* doc = score::test::new_document(app);
    REQUIRE(doc);
    auto& plug = doc->context().plugin<Gfx::DocumentPlugin>();
    auto& g = plug.context;
    auto& exec = plug.exec;

    auto isf = score::test::gfx::make_isf_node(corpus("isf-solid-color.fs"));
    REQUIRE(isf.node);
    auto sinkOwned = std::make_unique<score::gfx::BackgroundNode>();
    sinkOwned->shared_readback = std::make_shared<QRhiReadbackResult>();
    auto* sink = sinkOwned.get();
    const int32_t a = g.register_node(std::move(isf.node));
    const int32_t s = g.register_node(std::move(sinkOwned));
    {
      ossia::audio_tick_state st{};
      exec.startTick(st);
      exec.setEdge(
          Gfx::port_index{a, 0}, Gfx::port_index{s, 0},
          Process::CableType::ImmediateGlutton);
      exec.endTick(st);
    }
    g.updateGraph();
    g.renderFrames(3);
    if(!sink->renderer())
      return;

    auto* dev = makeDevice(doc->context());
    if(!dev)
      return;
    out.built = true;

    g_previousHandler = qInstallMessageHandler(collectWarnings);
    g_warnings.clear();
    {
      bool armed = true;
      sink->shared_readback->completed = [&] {
        if(!std::exchange(armed, false))
          return;
        out.calledInFrame = g.renderInProgress();
        dev->grabTo(png);
      };
      g.renderFrames(1);
      sink->shared_readback->completed = {};
    }
    out.inFrameRefusals = warningsContaining(
        QStringLiteral("score.gfx: grabTo refused: a frame is already being rendered"),
        QStringLiteral("Call grabTo outside a node's tick() or a readback callback."));

    g_warnings.clear();
    QTimer::singleShot(0, dev, [dev] { dev->disconnect(); });
    dev->grabTo(png);
    out.closedRefusals = warningsContaining(
        QStringLiteral("score.gfx: grabTo stopped: GrabRefusalE5 was closed during the grab"),
        QStringLiteral("Keep the device and its document open until grabTo returns"));
    delete dev;

    qInstallMessageHandler(g_previousHandler);
    g_previousHandler = {};
    out.written = QFile::exists(png);
  });
  return out;
}
}

TEST_CASE("grabTo refusals are actionable", "[gfx][window][grab][e5]")
{
  const auto api = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(api));

  const Outcome o = run(api);
  if(!o.skip.empty())
    SKIP(o.skip);
  REQUIRE(o.built);
  CHECK(o.calledInFrame);
  CHECK(o.inFrameRefusals == 1);
  CHECK(o.closedRefusals == 1);
  CHECK_FALSE(o.written);
}
