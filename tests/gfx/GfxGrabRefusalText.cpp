// WindowDevice::grabTo refusals name the entry point and say what to do.
//
// Two refusals: grabTo reached from inside a frame (here a sink's readback
// completion, which QRhi runs inside endOffscreenFrame while renderFrames is
// on the stack), and the device closed while grabTo pumps the event loop. Each
// writes nothing and logs one warning that starts with "score.gfx: grabTo" and
// tells the caller what to change.

#include "GfxLogCapture.hpp"
#include "IsfTestCommon.hpp"

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Settings/Model.hpp>
#include <Gfx/WindowDevice.hpp>

#include <core/document/Document.hpp>

#include <QFile>
#include <QTemporaryDir>
#include <QTimer>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

namespace
{
using score::test::gfx::isf::corpus;

constexpr auto kName = "GrabRefusal";

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
  std::string skip, error;
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
    // WindowDevice has no backend of its own: it renders with the one the
    // settings select, so each case selects the backend it tests.
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
    if(!doc)
    {
      out.error = "no document";
      return;
    }
    auto& plug = doc->context().plugin<Gfx::DocumentPlugin>();
    auto& g = plug.context;
    auto& exec = plug.exec;

    auto isf = score::test::gfx::make_isf_node(corpus("isf-solid-color.fs"));
    if(!isf.node)
    {
      out.error = isf.error;
      return;
    }
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
    {
      out.error = "the sink has no renderer after three frames";
      return;
    }

    auto* dev = makeDevice(doc->context());
    if(!dev)
    {
      out.error = "the offscreen window device did not connect";
      return;
    }

    score::test::gfx::LogCapture log;
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
    out.inFrameRefusals = log.count(
        QtWarningMsg,
        {u"score.gfx: grabTo refused: a frame is already being rendered",
         u"Call grabTo outside a node's tick() or a readback callback."});

    log.clear();
    QTimer::singleShot(0, dev, [dev] { dev->disconnect(); });
    dev->grabTo(png);
    out.closedRefusals = log.count(
        QtWarningMsg,
        {QStringLiteral("score.gfx: grabTo stopped: %1 was closed during the grab")
             .arg(QLatin1String(kName)),
         u"Keep the device and its document open until grabTo returns"});
    delete dev;
    out.written = QFile::exists(png);
  });
  qunsetenv("SCORE_FORCE_OFFSCREEN_WINDOW");
  return out;
}
}

TEST_CASE("grabTo refusals are actionable", "[gfx][window][grab]")
{
  const auto api = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(api));

  const Outcome o = run(api);
  if(!o.skip.empty())
    SKIP(o.skip);
  INFO(o.error);
  REQUIRE(o.error.empty());
  CHECK(o.calledInFrame);
  CHECK(o.inFrameRefusals == 1);
  CHECK(o.closedRefusals == 1);
  CHECK_FALSE(o.written);
}
