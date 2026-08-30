// UNIT — an output renderer whose pipeline failed to build must skip the draw,
// not hand a null QRhiGraphicsPipeline to the command buffer.
//
// score::gfx::buildPipeline returns a Pipeline with a null `pipeline` when
// QRhiGraphicsPipeline::create() fails. That is reachable in normal operation:
// createRenderTarget degrades to an empty target rather than aborting, and a
// pipeline built against a missing render pass fails to create — transiently,
// during a graph rebuild. InvertYRenderer::finishFrame and
// ScaledRenderer::finishFrame then reached
//
//     cb.setGraphicsPipeline(m_p.pipeline);
//
// with nullptr. QRhiCommandBuffer::setGraphicsPipeline is
// `Q_ASSERT(ps != nullptr); m_rhi->setGraphicsPipeline(this, ps);` — an abort
// where Qt keeps its assertions, and a dereference inside the backend
// (QRhiGles2 / QRhiVulkan read ps->m_shaderResourceBindings unconditionally)
// where it does not.
//
// SCOPE. This pins the *contract* — finishFrame tolerates a null pipeline and
// still clears and finishes the pass — by putting the renderer in the state
// buildPipeline leaves behind, not by driving a real pipeline creation to
// failure. m_p and m_renderTarget are public members that the owning sink
// assembles from outside, so the setup below is the same assignment init()
// performs, minus the pipeline. It does NOT cover the upstream question of
// *why* create() failed, nor that the null actually reaches here in a live
// graph — a render-target failure would have to be forced for that.
//
// BACKENDS. Every backend QRhi can bring up here is exercised, Null included:
// on a Qt built with assertions the Q_ASSERT above is the signal, on a real
// backend it is the dereference. A Null-only run against a Qt built with
// QT_NO_DEBUG would not fail on the defect — the CAPTURE below records which
// backend actually ran so a green result can be read for what it is worth.

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/Utils.hpp>
#include <Gfx/InvertYRenderer.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <QtGlobal>

#include <QtGui/private/qrhi_p.h>
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include <rhi/qrhi_platform.h>
#else
#include <QtGui/private/qrhigles2_p.h>
#include <QtGui/private/qrhinull_p.h>
#endif

#include <QGuiApplication>
#include <QOffscreenSurface>

#include <memory>

using namespace score::gfx;

namespace
{
// The QRhi backends that are not Null need a QGuiApplication: newFallbackSurface
// and the GL context both go through the platform integration. score_test's
// run_in_app / run_in_gui_app are the wrong shape for it -- they boot a whole
// score application (plug-in loading, settings, documents) around *one* call,
// so a test case that re-enters per generator value and per section would boot
// and tear down four of them, and this test wants none of it.
//
// It also must not be a function-local static, which is what this was: that
// defers ~QGuiApplication to __run_exit_handlers, and by then the thread-local
// storage QFontCache::cleanup() reaches for is gone -- the process segfaulted
// at exit *after* Catch2 had printed a green report. A listener owns it for
// exactly the length of the run and destroys it while Qt is still whole.
struct QtGuiApplication final : Catch::EventListenerBase
{
  using Catch::EventListenerBase::EventListenerBase;

  void testRunStarting(Catch::TestRunInfo const&) override
  {
    // What score::test::prepare_test_environment() does for the headless case:
    // with no platform plugin and no display, QGuiApplication aborts. Offscreen
    // still brings up a real GL context when there is a display to reach.
    if(!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
      qputenv("QT_QPA_PLATFORM", "offscreen");

    app = std::make_unique<QGuiApplication>(argc, argv);
  }

  void testRunEnded(Catch::TestRunStats const&) override { app.reset(); }

  int argc{1};
  char arg0[32] = "test_unit_output_null_pipeline";
  char* argv[2]{arg0, nullptr};
  std::unique_ptr<QGuiApplication> app;
};
CATCH_REGISTER_LISTENER(QtGuiApplication)

struct PlainNode final : Node
{
  NodeRenderer* createRenderer(RenderList&) const noexcept override { return nullptr; }
};

// See IsfUniformInputUsageTest: RenderList's constructor only stores the two
// references, and finishFrame reads state.renderSize / state.outputSize /
// state.rhi off it. Nothing here drives the sink.
struct StubOutput final : OutputNode
{
  OutputNodeRenderer* createRenderer(RenderList&) const noexcept override
  {
    return nullptr;
  }
  void setRenderer(std::shared_ptr<RenderList>) override { }
  RenderList* renderer() const override { return nullptr; }
  void startRendering() override { }
  void render() override { }
  void stopRendering() override { }
  bool canRender() const override { return false; }
  void onRendererChange() override { }
  void createOutput(OutputConfiguration) override { }
  void destroyOutput() override { }
  std::shared_ptr<RenderState> renderState() const override { return {}; }
  Configuration configuration() const noexcept override { return {}; }
};

constexpr QSize kSize{16, 16};

std::unique_ptr<QRhi> makeRhi(QRhi::Implementation impl, QOffscreenSurface*& glSurface)
{
  switch(impl)
  {
    case QRhi::Null:
    {
      QRhiNullInitParams p;
      return std::unique_ptr<QRhi>{QRhi::create(QRhi::Null, &p)};
    }
    case QRhi::OpenGLES2:
    {
      glSurface = QRhiGles2InitParams::newFallbackSurface();
      if(!glSurface)
        return {};
      QRhiGles2InitParams p;
      p.fallbackSurface = glSurface;
      return std::unique_ptr<QRhi>{QRhi::create(QRhi::OpenGLES2, &p)};
    }
    default:
      return {};
  }
}
}

TEST_CASE(
    "an output renderer skips the draw when its pipeline failed to build",
    "[gfx][output][pipeline][regression]")
{
  const auto impl = GENERATE(QRhi::Null, QRhi::OpenGLES2);

  // Declared first so it outlives the QRhi: QRhiGles2::destroy() makes the
  // fallback surface current one last time.
  std::unique_ptr<QOffscreenSurface> ownedSurface;
  std::unique_ptr<QRhi> rhi;
  {
    QOffscreenSurface* glSurface{};
    rhi = makeRhi(impl, glSurface);
    ownedSurface.reset(glSurface);
  }
  if(!rhi)
    SKIP("backend unavailable");

  CAPTURE(rhi->backendName());

  StubOutput sink;
  auto st = std::make_shared<RenderState>();
  st->rhi = rhi.get();
  st->renderSize = kSize;
  st->outputSize = kSize;
  st->renderFormat = QRhiTexture::RGBA8;
  st->samples = 1;
  RenderList rl{sink, st};

  PlainNode node;

  auto target = createRenderTarget(*st, QRhiTexture::RGBA8, kSize, 1, false);
  REQUIRE(target.renderTarget);
  REQUIRE(target.texture);

  SECTION("InvertYRenderer::finishFrame")
  {
    QRhiReadbackResult readback;
    Gfx::InvertYRenderer r{node, {}, readback};
    r.m_renderTarget = target;
    // Exactly what buildPipeline leaves behind on a create() failure.
    REQUIRE(r.m_p.pipeline == nullptr);

    QRhiCommandBuffer* cb{};
    REQUIRE(rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess);
    QRhiResourceUpdateBatch* res = rhi->nextResourceUpdateBatch();
    r.finishFrame(rl, *cb, res);
    rhi->endOffscreenFrame();

    // The pass still ran to completion: it cleared the target and read it
    // back. Pre-fix, control never got here.
    CHECK(readback.pixelSize == kSize);
    CHECK(readback.data.size() == kSize.width() * kSize.height() * 4);

    r.m_renderTarget = {};
    r.m_p = {};
  }

  SECTION("ScaledRenderer::finishFrame")
  {
    Gfx::ScaledRenderer r{target, *st, node};
    REQUIRE(r.m_p.pipeline == nullptr);

    QRhiCommandBuffer* cb{};
    REQUIRE(rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess);
    QRhiResourceUpdateBatch* res = rhi->nextResourceUpdateBatch();
    r.finishFrame(rl, *cb, res);
    rhi->endOffscreenFrame();

    // ScaledRenderer reads nothing back; surviving the pass IS the assertion,
    // so pin the one observable it leaves behind instead of a bare SUCCEED().
    CHECK(r.m_renderTarget.renderTarget == target.renderTarget);

    r.m_renderTarget = {};
    r.m_p = {};
  }

  target.release();
}
