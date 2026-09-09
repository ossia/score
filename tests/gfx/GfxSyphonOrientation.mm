// =============================================================================
// WHICH WAY UP Syphon hands a texture over, on OpenGL and on Metal.
//
// Syphon is the odd one out among score's video outputs: it shares the GPU
// texture rather than reading pixels back, so none of the flips the readback
// outputs apply are in play. Gfx::SyphonNode publishes the scene render target
// as it stands.
//
// The question that leaves open is whether a Syphon *client* then sees the
// picture the right way up, and the only honest way to answer it is to be one.
// So: one process, two graphs. The first paints a vertical ramp and publishes
// it through the real Gfx::SyphonNode; the second subscribes with the real
// Gfx::Syphon::SyphonInputNode and reads the result back through the same
// InvertYRenderer path every other output uses -- the one
// GfxVideoOutputOrientation.cpp pins as delivering row 0 == top.
//
// A Syphon server needs a window-server session. Launched from ssh without
// one, the server never appears in the directory and the test SKIPs saying so
// rather than passing on nothing.
//
//   ./test_gfx_syphon_orientation
//   SCORE_TEST_API=opengl ./test_gfx_syphon_orientation
// =============================================================================

#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/TexgenNode.hpp>
#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/SharedInputSettings.hpp>
#include <Gfx/SharedOutputSettings.hpp>
#include <Gfx/Syphon/SyphonInput.hpp>
#include <Gfx/Syphon/SyphonOutput.hpp>

#include <score_test/App.hpp>
#include <score_test/Gfx.hpp>

#include <QElapsedTimer>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <Syphon/SyphonServerDirectory.h>

using namespace score::test::gfx;

namespace
{
constexpr int kW = 64;
constexpr int kH = 64;

//! Row 0 black, row H-1 white. Row 0 is the top.
void paintVerticalRamp(unsigned char* rgba, int w, int h, int)
{
  for(int y = 0; y < h; y++)
  {
    const auto v = (unsigned char)std::lround(255.0 * double(y) / double(h - 1));
    for(int x = 0; x < w; x++)
    {
      auto* p = rgba + (std::size_t(y) * w + x) * 4;
      p[0] = v;
      p[1] = v;
      p[2] = v;
      p[3] = 255;
    }
  }
}

//! The InvertYRenderer readback sink, which GfxVideoOutputOrientation pins as
//! delivering row 0 == top on every backend.
class ReadbackSink final : public score::gfx::OutputNode
{
public:
  ReadbackSink()
  {
    input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
  }
  ~ReadbackSink() override { }

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
    return {.manualRenderingRate = 1000. / 30.};
  }

  void createOutput(score::gfx::OutputConfiguration conf) override
  {
    m_renderState
        = score::gfx::createRenderState(conf.graphicsApi, QSize(kW, kH), nullptr);
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
    m_texture->create();
    m_depthStencil = rhi->newRenderBuffer(
        QRhiRenderBuffer::DepthStencil, m_renderState->renderSize, 1);
    m_depthStencil->create();
    QRhiTextureRenderTargetDescription desc{m_texture};
    desc.setDepthStencilBuffer(m_depthStencil);
    m_renderTarget = rhi->newTextureRenderTarget(desc);
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
    delete m_depthStencil;
    m_depthStencil = nullptr;
    delete m_texture;
    m_texture = nullptr;
    m_renderState->destroy();
    m_renderState.reset();
  }

  score::gfx::OutputNodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override
  {
    score::gfx::TextureRenderTarget rt{
        .texture = m_texture,
        .renderPass = m_renderState->renderPassDescriptor,
        .renderTarget = m_renderTarget};
    return new Gfx::InvertYRenderer{
        *this, rt, const_cast<QRhiReadbackResult&>(m_readback)};
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

  std::vector<int> lumaRows() const
  {
    std::vector<int> rows;
    if(m_readback.data.isEmpty())
      return rows;
    const auto* d = reinterpret_cast<const uint8_t*>(m_readback.data.constData());
    const int h = m_readback.pixelSize.height();
    if(h <= 0)
      return rows;
    const int stride = m_readback.data.size() / h;
    for(int y = 0; y < h; y++)
      rows.push_back(d[std::size_t(y) * stride + (kW / 2) * 4 + 1]);
    return rows;
  }

private:
  std::weak_ptr<score::gfx::RenderList> m_renderer{};
  QRhiTexture* m_texture{};
  QRhiRenderBuffer* m_depthStencil{};
  QRhiTextureRenderTarget* m_renderTarget{};
  std::shared_ptr<score::gfx::RenderState> m_renderState{};
  QRhiReadbackResult m_readback{};
};

//! UUID of the first Syphon server this process is advertising, or empty.
QString ownServerUuid()
{
  SyphonServerDirectory* dir = [SyphonServerDirectory sharedDirectory];
  NSArray* servers = [dir serversMatchingName:NULL appName:NULL];
  for(NSUInteger i = 0; i < servers.count; i++)
  {
    NSDictionary* s = servers[i];
    NSString* uuid = s[SyphonServerDescriptionUUIDKey];
    if(uuid)
      return QString::fromNSString(uuid);
  }
  return {};
}

struct Shot
{
  bool skipped{};
  std::string why;
  std::vector<int> rows;
};

Shot runSyphonRoundTrip(score::gfx::GraphicsApi api)
{
  Shot out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    // --- publisher ---
    Gfx::SharedOutputSettings oset;
    oset.path = "score-orientation";
    oset.width = kW;
    oset.height = kH;
    oset.rate = 30.;

    auto* src = new score::gfx::TexgenNode;
    src->function = &paintVerticalRamp;
    score::gfx::OutputNode* pub = Gfx::makeSyphonOutput(oset);

    auto pubGraph = std::make_unique<score::gfx::Graph>();
    pubGraph->addNode(src);
    pubGraph->addNode(pub);
    pubGraph->addEdge(
        src->output[0], pub->input[0], Process::CableType::ImmediateGlutton);
    pubGraph->createAllRenderLists(api);

    if(!pub->canRender())
    {
      out.skipped = true;
      out.why = "the Syphon publisher could not bring up its output";
      pubGraph.reset();
      delete pub;
      delete src;
      return;
    }

    for(int i = 0; i < 8; i++)
      pub->render();

    // The directory is populated through the run loop.
    QElapsedTimer t;
    t.start();
    QString uuid;
    while(uuid.isEmpty() && t.elapsed() < 4000)
    {
      QApplication::processEvents(QEventLoop::AllEvents, 20);
      pub->render();
      uuid = ownServerUuid();
    }

    if(uuid.isEmpty())
    {
      out.skipped = true;
      out.why = "no Syphon server appeared in the directory: this process has "
                "no window-server session";
      pubGraph.reset();
      delete pub;
      delete src;
      return;
    }

    // --- subscriber ---
    Gfx::SharedInputSettings iset;
    iset.path = uuid;
    auto* sub = Gfx::Syphon::makeSyphonInput(iset);
    auto* sink = new ReadbackSink;

    auto subGraph = std::make_unique<score::gfx::Graph>();
    subGraph->addNode(sub);
    subGraph->addNode(sink);
    subGraph->addEdge(
        sub->output[0], sink->input[0], Process::CableType::ImmediateGlutton);
    subGraph->createAllRenderLists(api);

    if(sink->canRender())
    {
      // Keep publishing while the receiver spins up: the client connects, then
      // waits for a frame.
      for(int i = 0; i < 40; i++)
      {
        pub->render();
        sink->render();
        QApplication::processEvents(QEventLoop::AllEvents, 5);
      }
      out.rows = sink->lumaRows();
    }
    else
    {
      out.skipped = true;
      out.why = "the Syphon receiver could not bring up its render list";
    }

    subGraph.reset();
    delete sink;
    delete sub;
    pubGraph.reset();
    delete pub;
    delete src;
  });
  return out;
}
}

TEST_CASE("Syphon hands over a top-down picture", "[gfx][syphon][orientation]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto shot = runSyphonRoundTrip(api);

  INFO("backend " << backend_name(api));
  if(shot.skipped)
    SKIP(shot.why);

  REQUIRE(shot.rows.size() >= 8);
  const auto [lo, hi] = std::minmax_element(shot.rows.begin(), shot.rows.end());
  INFO("first " << shot.rows.front() << ", last " << shot.rows.back());
  REQUIRE((*hi - *lo) > 60);

  // The ramp was painted black at row 0. Through the readback sink that
  // GfxVideoOutputOrientation pins as row 0 == top, it must come back the same
  // way round: anything else is a flip somewhere in the Syphon share.
  CHECK(shot.rows.front() < shot.rows.back());

  // ... and per row, not just at the ends.
  const double span = double(*hi - *lo);
  int worst = 0, worstRow = 0;
  for(std::size_t y = 0; y < shot.rows.size(); y++)
  {
    const double t = double(y) / double(shot.rows.size() - 1);
    const int expected = int(std::lround(*lo + span * t));
    if(const int d = std::abs(shot.rows[y] - expected); d > worst)
    {
      worst = d;
      worstRow = int(y);
    }
  }
  INFO("worst row " << worstRow << ": " << shot.rows[worstRow]);
  CHECK(worst <= 24);
}
