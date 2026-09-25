// A render entry point reached while a frame is being recorded does nothing.
//
// One ISF producer feeds two offscreen sinks of the real GfxContext, each on its
// own QRhi. From inside a frame, at two points, the test reaches every entry
// point that can render: GfxContext::renderFrames, a pumped event loop in which
// the render clock and the graph-update timer are due, and, inside
// RenderList::render, a second render of the same list on the same command
// buffer. The two points are a sink renderer's finishFrame(), which runs inside
// RenderList::render, and the other sink's readback completion, which QRhi runs
// inside endOffscreenFrame after the list has finished.
//
// Asserted: no sink's render() is called and no frame is recorded while the
// outer frame is open, QRhi never sees a begin inside an active frame, the outer
// frame counts as one frame, and frames go on being produced afterwards with the
// producer's colour in the readback. The same pump outside a frame does render
// through the clock, so the pump inside one had ticks due.
//
// Registration: see test_gfx_render_reentry_g9.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Gfx.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/Settings/Model.hpp>

#include <core/document/Document.hpp>

#include <QCoreApplication>
#include <QStringList>
#include <QThread>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <functional>

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

int warningsContaining(const char* text)
{
  int n = 0;
  for(const auto& w : g_warnings)
    if(w.contains(QString::fromUtf8(text)))
      n++;
  return n;
}

struct Counters
{
  int renders{};
};
Counters g_sinks[2];

std::function<void(score::gfx::RenderList&)> g_inFinishFrame;

struct HookRenderer final : Gfx::BasicRenderer
{
  using Gfx::BasicRenderer::BasicRenderer;
  void finishFrame(
      score::gfx::RenderList& rl, QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch*& res) override
  {
    Gfx::BasicRenderer::finishFrame(rl, cb, res);
    if(auto f = std::exchange(g_inFinishFrame, {}))
      f(rl);
  }
};

struct CountingSink : score::gfx::BackgroundNode
{
  explicit CountingSink(int slot)
      : m_slot{slot}
  {
    shared_readback = std::make_shared<QRhiReadbackResult>();
  }
  void render() override
  {
    ++g_sinks[m_slot].renders;
    score::gfx::BackgroundNode::render();
  }
  int m_slot{};
};

struct HookSink final : CountingSink
{
  using CountingSink::CountingSink;
  score::gfx::OutputNodeRenderer*
  createRenderer(score::gfx::RenderList&) const noexcept override
  {
    return new HookRenderer{currentRenderTarget(), *renderState(), *this};
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

struct Nested
{
  int rendersFromRenderFrames{-1};
  int rendersFromPump{-1};
  int64_t hookFrames{-1};
  int64_t otherFrames{-1};
  bool ran{};
};

struct Outcome
{
  bool built{};
  std::string skip;
  Nested inFinishFrame;
  Nested inReadback;
  int64_t hookOuterFrames{-1};
  int64_t readbackOuterFrames{-1};
  int nestedBegins{-1};
  int skippedEntries{-1};
  int64_t framesAfter[2]{-1, -1};
  int clockRendersOutsideFrames{-1};
  bool magenta{};
};

void provoke(
    Gfx::GfxContext& g, score::gfx::RenderList& hook, score::gfx::RenderList& other,
    Nested& out, bool renderSameList)
{
  const auto renders = [] { return g_sinks[0].renders + g_sinks[1].renders; };
  const int64_t hookBefore = hook.frame;
  const int64_t otherBefore = other.frame;

  int before = renders();
  g.renderFrames(1);
  out.rendersFromRenderFrames = renders() - before;

  before = renders();
  QThread::msleep(60);
  for(int i = 0; i < 8; i++)
    QCoreApplication::processEvents(QEventLoop::AllEvents);
  out.rendersFromPump = renders() - before;

  if(renderSameList)
    if(auto* cb = hook.currentCommandBuffer())
      hook.render(*cb);

  out.hookFrames = hook.frame - hookBefore;
  out.otherFrames = other.frame - otherBefore;
  out.ran = true;
}

Outcome run(score::gfx::GraphicsApi backend)
{
  Outcome out;
  g_sinks[0] = {};
  g_sinks[1] = {};
  g_inFinishFrame = {};

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

    auto hookOwned = std::make_unique<HookSink>(0);
    auto otherOwned = std::make_unique<CountingSink>(1);
    HookSink* hookSink = hookOwned.get();
    CountingSink* otherSink = otherOwned.get();

    const int32_t a = g.register_node(std::move(isf.node));
    const int32_t s0 = g.register_node(std::move(hookOwned));
    const int32_t s1 = g.register_node(std::move(otherOwned));
    {
      ossia::audio_tick_state st{};
      exec.startTick(st);
      exec.setEdge(
          Gfx::port_index{a, 0}, Gfx::port_index{s0, 0},
          Process::CableType::ImmediateGlutton);
      exec.setEdge(
          Gfx::port_index{a, 0}, Gfx::port_index{s1, 0},
          Process::CableType::ImmediateGlutton);
      exec.endTick(st);
    }
    g.updateGraph();
    g.renderFrames(3);

    auto* hookRl = hookSink->renderer();
    auto* otherRl = otherSink->renderer();
    if(!hookRl || !otherRl || hookRl->renderers.size() < 2
       || otherRl->renderers.size() < 2)
      return;
    out.built = true;

    g_previousHandler = qInstallMessageHandler(collectWarnings);
    g_warnings.clear();

    {
      g_inFinishFrame = [&](score::gfx::RenderList& rl) {
        provoke(g, rl, *otherRl, out.inFinishFrame, true);
      };
      const int64_t before = hookRl->frame;
      g.renderFrames(1);
      out.hookOuterFrames = hookRl->frame - before;
      g_inFinishFrame = {};
    }

    {
      bool armed = true;
      otherSink->shared_readback->completed = [&] {
        if(!std::exchange(armed, false))
          return;
        provoke(g, *otherRl, *hookRl, out.inReadback, false);
      };
      const int64_t before = otherRl->frame;
      g.renderFrames(1);
      out.readbackOuterFrames = otherRl->frame - before;
      otherSink->shared_readback->completed = {};
    }

    out.nestedBegins = warningsContaining("within a still active frame")
                       + warningsContaining("entered while a frame is already recording");
    out.skippedEntries = warningsContaining("while a frame is being rendered")
                         + warningsContaining("while this list is already rendering");

    qInstallMessageHandler(g_previousHandler);
    g_previousHandler = {};

    const int64_t h0 = hookRl->frame;
    const int64_t o0 = otherRl->frame;
    g.renderFrames(4);
    out.framesAfter[0] = hookRl->frame - h0;
    out.framesAfter[1] = otherRl->frame - o0;

    {
      const int before = g_sinks[0].renders + g_sinks[1].renders;
      QThread::msleep(60);
      for(int i = 0; i < 8; i++)
        QCoreApplication::processEvents(QEventLoop::AllEvents);
      out.clockRendersOutsideFrames = g_sinks[0].renders + g_sinks[1].renders - before;
    }

    const auto& rb = *otherSink->shared_readback;
    const int w = rb.pixelSize.width();
    const int h = rb.pixelSize.height();
    if(w > 0 && h > 0 && rb.data.size() >= w * h * 4)
    {
      const auto* px = reinterpret_cast<const unsigned char*>(rb.data.constData())
                       + ((h / 2) * w + w / 2) * 4;
      out.magenta = px[0] > 200 && px[1] < 50 && px[2] > 200;
    }
  });
  return out;
}
}

TEST_CASE(
    "a render entry point reached inside a frame does nothing",
    "[gfx][render][reentry][g9]")
{
  const auto backend = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(backend));

  const Outcome o = run(backend);
  if(!o.skip.empty())
    SKIP(o.skip);
  REQUIRE(o.built);

  REQUIRE(o.inFinishFrame.ran);
  REQUIRE(o.inReadback.ran);

  CHECK(o.clockRendersOutsideFrames > 0);

  CHECK(o.inFinishFrame.rendersFromRenderFrames == 0);
  CHECK(o.inFinishFrame.rendersFromPump == 0);
  CHECK(o.inFinishFrame.hookFrames == 0);
  CHECK(o.inFinishFrame.otherFrames == 0);
  CHECK(o.inReadback.rendersFromRenderFrames == 0);
  CHECK(o.inReadback.rendersFromPump == 0);
  CHECK(o.inReadback.hookFrames == 0);
  CHECK(o.inReadback.otherFrames == 0);

  CHECK(o.hookOuterFrames == 1);
  CHECK(o.readbackOuterFrames == 1);

  CHECK(o.nestedBegins == 0);
  CHECK(o.skippedEntries > 0);

  CHECK(o.framesAfter[0] == 4);
  CHECK(o.framesAfter[1] == 4);
  CHECK(o.magenta);
}
