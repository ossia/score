// What a frame of the scene render costs as the picture grows, with no readback
// and no output device in the way.
//
// The PipeWire dma-buf output was spending orders of magnitude more per frame
// at 8K than its handover -- dequeue, wrap, begin, wait -- accounts for. This
// case measures the other two candidates and rules both out: drawing an 8K
// frame is cheap, and copying it from one ordinary texture to another is
// cheap. What is left is the one operation this case does NOT perform: writing
// into an EXPORTED image, the dedicated external allocation that carries the
// dma-buf. That write is slow whatever the memory type and whatever the tiling
// -- both were measured -- against the ordinary copy above.
//
// One animated ISF, one offscreen target, no readback, no device. Every frame
// is bracketed by beginOffscreenFrame/endOffscreenFrame and endOffscreenFrame
// waits for the GPU, so the number is the frame's real cost and not the cost of
// recording it.
//
// The last frame IS read back, once, outside the timed loop: a timing harness
// that draws nothing reports a wonderful number, and this one would have --
// the readback is here so the case fails instead of lying.

#include "IsfTestCommon.hpp"

#include <score_test/App.hpp>

#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/InvertYRenderer.hpp>

#include <QElapsedTimer>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <set>
#include <vector>

using namespace score::test::gfx;

namespace
{
//! Renders and leaves the result on the GPU. The readback is a separate,
//! explicit step so it never lands inside a measurement.
class SilentSink final : public score::gfx::OutputNode
{
public:
  explicit SilentSink(QSize sz)
      : m_size{sz}
  {
    input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
  }
  ~SilentSink() override { }

  bool canRender() const override { return bool(m_renderState); }
  void startRendering() override { }
  void stopRendering() override { }
  void onRendererChange() override { }
  void setRenderer(std::shared_ptr<score::gfx::RenderList> r) override
  {
    m_renderer = r;
  }
  score::gfx::RenderList* renderer() const override { return m_renderer.lock().get(); }
  std::shared_ptr<score::gfx::RenderState> renderState() const override
  {
    return m_renderState;
  }
  Configuration configuration() const noexcept override
  {
    return {.manualRenderingRate = 1000. / 60.};
  }

  void createOutput(score::gfx::OutputConfiguration conf) override
  {
    m_renderState = score::gfx::createRenderState(conf.graphicsApi, m_size, nullptr);
    if(!m_renderState || !m_renderState->rhi)
    {
      m_renderState.reset();
      return;
    }
    m_renderState->outputSize = m_renderState->renderSize;
    auto rhi = m_renderState->rhi;
    m_renderState->renderFormat = QRhiTexture::RGBA8;
    m_texture = rhi->newTexture(
        QRhiTexture::RGBA8, m_renderState->renderSize, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    if(!m_texture->create())
    {
      m_renderState.reset();
      return;
    }
    m_renderTarget = rhi->newTextureRenderTarget({m_texture});
    m_renderState->renderPassDescriptor
        = m_renderTarget->newCompatibleRenderPassDescriptor();
    m_renderTarget->setRenderPassDescriptor(m_renderState->renderPassDescriptor);
    m_renderTarget->create();
    if(conf.onReady)
      conf.onReady();
  }

  void destroyOutput() override
  {
    if(!m_renderState)
      return;
    delete m_renderTarget;
    m_renderTarget = nullptr;
    delete m_renderState->renderPassDescriptor;
    m_renderState->renderPassDescriptor = nullptr;
    delete m_texture;
    m_texture = nullptr;
    m_renderState->destroy();
    m_renderState.reset();
  }

  score::gfx::OutputNodeRenderer*
  createRenderer(score::gfx::RenderList&) const noexcept override
  {
    return new Gfx::BasicRenderer{
        score::gfx::TextureRenderTarget{
            .texture = m_texture,
            .renderPass = m_renderState->renderPassDescriptor,
            .renderTarget = m_renderTarget},
        *m_renderState, *this};
  }

  void render() override
  {
    auto renderer = m_renderer.lock();
    if(!renderer || !m_renderState)
      return;
    auto rhi = m_renderState->rhi;
    QRhiCommandBuffer* cb{};
    if(rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
      return;
    renderer->render(*cb);
    rhi->endOffscreenFrame();
  }

  //! Render, then copy the result into a second ORDINARY texture of the same
  //! size. The PipeWire path makes the same copy into an exported image and
  //! pays far more for it at 8K; this is the control that says the copy is not
  //! what costs, the export is.
  void renderAndCopy()
  {
    auto renderer = m_renderer.lock();
    if(!renderer || !m_renderState)
      return;
    auto rhi = m_renderState->rhi;
    if(!m_copyTarget)
    {
      m_copyTarget = rhi->newTexture(
          QRhiTexture::RGBA8, m_renderState->renderSize, 1,
          QRhiTexture::UsedAsTransferSource);
      m_copyTarget->create();
    }
    QRhiCommandBuffer* cb{};
    if(rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
      return;
    renderer->render(*cb);
    auto* batch = rhi->nextResourceUpdateBatch();
    QRhiTextureCopyDescription cdesc;
    cdesc.setPixelSize(m_renderState->renderSize);
    batch->copyTexture(m_copyTarget, m_texture, cdesc);
    cb->resourceUpdate(batch);
    rhi->endOffscreenFrame();
  }

  //! One frame, read back. Proves the timed loop was drawing something.
  QByteArray grab()
  {
    auto renderer = m_renderer.lock();
    if(!renderer || !m_renderState)
      return {};
    auto rhi = m_renderState->rhi;
    QRhiCommandBuffer* cb{};
    if(rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
      return {};
    renderer->render(*cb);

    QRhiReadbackResult rb;
    auto* batch = rhi->nextResourceUpdateBatch();
    batch->readBackTexture(QRhiReadbackDescription{m_texture}, &rb);
    cb->resourceUpdate(batch);
    rhi->endOffscreenFrame();
    return rb.data;
  }

private:
  QSize m_size;
  std::weak_ptr<score::gfx::RenderList> m_renderer{};
  QRhiTexture* m_texture{};
  QRhiTextureRenderTarget* m_renderTarget{};
  QRhiTexture* m_copyTarget{};
  std::shared_ptr<score::gfx::RenderState> m_renderState{};
};

struct Sample
{
  bool ok{};
  bool drew{};
  double msPerFrame{};
  double msPerFrameWithCopy{};
  double megapixels{};
  double msPerMegapixel() const { return megapixels > 0 ? msPerFrame / megapixels : 0; }
};

//! True if the readback holds more than one colour: a target that was never
//! drawn into comes back uniform.
bool hasContent(const QByteArray& px)
{
  if(px.size() < 64)
    return false;
  std::set<uint32_t> seen;
  const auto* p = reinterpret_cast<const uint32_t*>(px.constData());
  const int n = int(px.size() / 4);
  for(int i = 0; i < n; i += 997)
  {
    seen.insert(p[i]);
    if(seen.size() > 2)
      return true;
  }
  return seen.size() > 1;
}

Sample timeFrames(score::gfx::GraphicsApi api, QSize size, int frames)
{
  Sample out;
  out.megapixels = double(size.width()) * size.height() / 1e6;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    auto isf
        = make_isf_node(QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-time-uniforms.fs"));
    if(!isf.node)
      return;
    auto* sink = new SilentSink{size};

    auto graph = std::make_unique<score::gfx::Graph>();
    graph->addNode(isf.node.get());
    graph->addNode(sink);
    graph->addEdge(
        isf.node->output[0], sink->input[0], Process::CableType::ImmediateGlutton);
    graph->createAllRenderLists(api);

    if(sink->canRender())
    {
      // Warm: the first frames build pipelines and upload the shader.
      for(int i = 0; i < 5; i++)
        sink->render();

      QElapsedTimer t;
      t.start();
      for(int i = 0; i < frames; i++)
        sink->render();
      out.msPerFrame = double(t.nsecsElapsed()) / 1e6 / frames;

      for(int i = 0; i < 3; i++)
        sink->renderAndCopy();
      t.restart();
      for(int i = 0; i < frames; i++)
        sink->renderAndCopy();
      out.msPerFrameWithCopy = double(t.nsecsElapsed()) / 1e6 / frames;
      out.ok = true;

      out.drew = hasContent(sink->grab());
    }

    graph.reset();
    delete sink;
  });
  return out;
}
}

TEST_CASE("the scene render's cost per pixel holds as the target grows", "[gfx][perf]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  constexpr int kFrames = 50;
  const auto hd = timeFrames(api, {1920, 1080}, kFrames);
  const auto uhd = timeFrames(api, {3840, 2160}, kFrames);
  const auto k8 = timeFrames(api, {7680, 4320}, kFrames);

  INFO("backend " << backend_name(api));
  if(!hd.ok)
    SKIP("no usable GPU for a 1080p offscreen render here");
  if(!uhd.ok || !k8.ok)
    SKIP("the GPU could not bring up a 4K or 8K offscreen target");

  INFO(
      "1080p " << hd.msPerFrame << " ms/frame (" << hd.msPerMegapixel()
               << " ms/Mpix), 4K " << uhd.msPerFrame << " ms ("
               << uhd.msPerMegapixel() << "), 8K " << k8.msPerFrame << " ms ("
               << k8.msPerMegapixel() << ")");
  INFO(
      "with a full-size GPU copy on top: 1080p " << hd.msPerFrameWithCopy
                                                 << " ms, 4K "
                                                 << uhd.msPerFrameWithCopy
                                                 << " ms, 8K "
                                                 << k8.msPerFrameWithCopy << " ms");

  // The measurement is only worth reading if the frames existed.
  REQUIRE(hd.drew);
  REQUIRE(uhd.drew);
  REQUIRE(k8.drew);

  REQUIRE(hd.msPerFrame > 0.);
  REQUIRE(k8.msPerFrame > 0.);

  // The shape, not the speed: cost per megapixel must not run away with the
  // target. A slower machine still passes; a renderer that degrades
  // superlinearly does not.
  CHECK(k8.msPerMegapixel() <= hd.msPerMegapixel() * 3.);
  CHECK(uhd.msPerMegapixel() <= hd.msPerMegapixel() * 3.);
}
