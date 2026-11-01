// Where a preview of a texture outlet is rendered: by the output of the patch
// that renders the outlet, else by the shared output of the previews, and back
// as outputs come and go. Drives the real document GfxContext.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Gfx.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/PreviewHubNode.hpp>
#include <Gfx/Widgets/TexturePreview.hpp>

#include <Gfx/Settings/Model.hpp>

#include <core/document/Document.hpp>

#include <QMutex>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <atomic>
#include <string>
#include <thread>

namespace
{
// Before the process' Vulkan instance is created: the layer reports a pass
// recorded into a freed target. An explicit setting wins.
const bool g_validation = [] {
  if(!qEnvironmentVariableIsSet("SCORE_GPU_VALIDATION"))
    qputenv("SCORE_GPU_VALIDATION", "1");
  return true;
}();

//! Counts the validation layer's errors, which QVulkanInstance logs through Qt
struct ValidationErrors
{
  static inline std::atomic_int count{};
  static inline QtMessageHandler previous{};
  static inline QMutex lock;
  static inline std::string first;

  ValidationErrors()
  {
    count = 0;
    first.clear();
    previous = qInstallMessageHandler(&handler);
  }
  ~ValidationErrors() { qInstallMessageHandler(previous); }

  static void handler(QtMsgType t, const QMessageLogContext& ctx, const QString& msg)
  {
    // Qt's own staging images are created PREINITIALIZED with optimal tiling,
    // which the layer reports at every texture upload: not ours
    const bool layer = msg.startsWith(QLatin1String("vkDebug:"))
                       || msg.contains(QLatin1String("Validation Error"));
    if(layer && !msg.contains(QLatin1String("VK_IMAGE_LAYOUT_PREINITIALIZED"))
       && !msg.contains(QLatin1String("duplicate_message_limit")))
    {
      if(count++ == 0)
      {
        QMutexLocker l{&lock};
        first = msg.toStdString();
      }
    }
    if(previous)
      previous(t, ctx, msg);
  }
};

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

struct Step
{
  bool tapped{};
  bool onHub{};
  QColor center;
};
}

TEST_CASE(
    "a preview moves between the shared output and the outputs of the patch",
    "[gfx][preview][hub][gui]")
{
  bool live = false;
  Step alone, withOutput, outputGone;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& plug = doc->context().plugin<Gfx::DocumentPlugin>();
    auto& g = plug.context;
    auto& exec = plug.exec;

    auto built = score::test::gfx::make_isf_node(
        QString{GFX_TEST_CORPUS_DIR "/isf-solid-color.fs"});
    REQUIRE(built.node);
    const int32_t p = g.register_node(std::move(built.node));

    const auto publish = [&](std::vector<Gfx::EdgeSpec> es) {
      ossia::audio_tick_state st{};
      exec.startTick(st);
      for(const auto& e : es)
        exec.setEdge(e.first, e.second, e.type);
      exec.endTick(st);
    };
    // Renders what the clocks would, then waits for the readbacks
    const auto renderAll = [&] {
      for(int i = 0; i < 4; i++)
      {
        g.updateGraph();
        for(auto* out : g.outputs())
        {
          out->render();
          if(auto rs = out->renderState(); rs && rs->rhi)
            rs->rhi->finish();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
      }
    };
    const auto step = [&](const Gfx::TexturePreview& tp) {
      Step s;
      s.tapped = tp.tapped();
      s.onHub = tp.onHub();
      const QImage img = tp.image();
      if(!img.isNull())
        s.center = img.pixelColor(img.width() / 2, img.height() / 2);
      return s;
    };

    Gfx::TexturePreview tp;
    tp.attach(&g, Gfx::port_index{p, 0}, QSize{32, 32});
    renderAll();
    alone = step(tp);

    // An output of the patch starts rendering the producer
    auto out = std::make_unique<score::gfx::BackgroundNode>();
    out->shared_readback = std::make_shared<QRhiReadbackResult>();
    out->setSize({64, 64});
    auto* sink = out.get();
    const int32_t s = g.register_node(std::move(out));
    const Gfx::EdgeSpec e{{p, 0}, {s, 0}, Process::CableType::ImmediateGlutton};
    publish({e});
    renderAll();
    live = sink->renderer() != nullptr;
    withOutput = step(tp);

    // ... and stops
    publish({});
    renderAll();
    outputGone = step(tp);

    tp.detach();
    g.unregister_node(s);
    g.unregister_node(p);
    g.updateGraph();
  });

  if(!live)
    SKIP("no working RHI here: the output never came up");

  const QColor magenta{255, 0, 255};
  CHECK(alone.onHub);
  CHECK_FALSE(alone.tapped);
  CHECK(alone.center == magenta);

  CHECK(withOutput.tapped);
  CHECK_FALSE(withOutput.onHub);
  CHECK(withOutput.center == magenta);

  CHECK(outputGone.onHub);
  CHECK(outputGone.center == magenta);
}

TEST_CASE(
    "resizing the smaller of two previews on the shared output rebuilds what draws "
    "into it",
    "[gfx][preview][hub][gui]")
{
  const auto api = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(api));

  std::string skip;
  bool hubbed = false;
  QSize before, after, larger;
  QColor center;
  std::vector<QColor> corners;
  int errors = 0;
  std::string firstError;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    if(api == score::gfx::Null)
    {
      skip = score::test::gfx::null_backend_skip_reason();
      return;
    }
    std::string probed;
    if(!score::test::gfx::probe_api(api, probed))
    {
      skip = std::string(score::test::gfx::backend_name(api)) + " unavailable";
      return;
    }
    auto& settings = ctx.settings<Gfx::Settings::Model>();
    settings.setVSync(false);
    settings.setGraphicsApi(apiSettingName(api));

    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& g = doc->context().plugin<Gfx::DocumentPlugin>().context;

    auto a = score::test::gfx::make_isf_node(
        QString{GFX_TEST_CORPUS_DIR "/isf-solid-color.fs"});
    auto b = score::test::gfx::make_isf_node(
        QString{GFX_TEST_CORPUS_DIR "/isf-solid-color.fs"});
    REQUIRE(a.node);
    REQUIRE(b.node);
    const int32_t pa = g.register_node(std::move(a.node));
    const int32_t pb = g.register_node(std::move(b.node));

    ValidationErrors validation;
    const auto renderAll = [&] {
      for(int i = 0; i < 4; i++)
      {
        g.updateGraph();
        for(auto* out : g.outputs())
        {
          out->render();
          if(auto rs = out->renderState(); rs && rs->rhi)
            rs->rhi->finish();
        }
      }
    };

    // Two previews, each a chain of its own on the hub; the larger one sets
    // the render size of the hub's list
    Gfx::TexturePreview small, big;
    small.attach(&g, Gfx::port_index{pa, 0}, QSize{32, 32});
    big.attach(&g, Gfx::port_index{pb, 0}, QSize{96, 96});
    renderAll();
    hubbed = small.onHub() && big.onHub();
    before = small.image().size();

    // The render size stays: only the smaller input's target changes
    small.setSize(QSize{48, 48});
    renderAll();
    (void)small.image();
    renderAll();
    const QImage img = small.image();
    after = img.size();
    if(!img.isNull())
    {
      center = img.pixelColor(img.width() / 2, img.height() / 2);
      // The whole of the new target is drawn, not the old size of it
      const int w = img.width() - 2, h = img.height() - 2;
      for(auto [x, y] : {std::pair{1, 1}, {w, 1}, {1, h}, {w, h}})
        corners.push_back(img.pixelColor(x, y));
    }
    larger = big.image().size();

    small.detach();
    big.detach();
    g.unregister_node(pa);
    g.unregister_node(pb);
    g.updateGraph();
    errors = ValidationErrors::count;
    firstError = ValidationErrors::first;
  });

  if(!skip.empty())
    SKIP(skip);
  if(!hubbed)
    SKIP("no working RHI here: the previews never reached the shared output");

  CHECK(before == QSize(32, 32));
  CHECK(after == QSize(48, 48));
  CHECK(larger == QSize(96, 96));
  CHECK(center == QColor(255, 0, 255));
  for(const auto& c : corners)
  {
    CAPTURE(c.red(), c.green(), c.blue());
    CHECK(c == QColor(255, 0, 255));
  }
  CAPTURE(firstError);
  CHECK(errors == 0);
}
