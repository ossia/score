// An offscreen output renders at the render list's sample count, the way a
// window's multisampled swap chain does, and hands its readers the resolved
// picture.
//
// The Samples setting sets RenderState::samples for every output. A window
// creates its swap chain at that count; the offscreen outputs (NDI, shmdata,
// sh4lt, PipeWire, Spout, Syphon, the libav and GStreamer encoders, capture
// cards, texture previews) draw into a target of their own, which
// score::gfx::createOutputRenderTarget builds: multisampled, resolved into the
// single-sample texture their encoders, readbacks and texture sharing read.
// On OpenGL the windowless QRhi these outputs create did not even come up at
// Samples > 1: the fallback surface and the context disagreed on the sample
// count, and every one of them sent the Null backend's garbage.
//
// The scene is a white square turned so that none of its edges follows the
// pixel grid, drawn by an ISF vertex shader straight into the output. Over
// black, a single-sample target only holds black and white; at four samples the
// pixels the edges cross come out grey. Checked through the three ways an
// output gets its pixels out:
//
//   preview   BackgroundNode: Gfx::InvertYRenderer + readback (texture
//             previews, the offscreen window device, shmdata, sh4lt)
//   sink      SinkNode: Gfx::BasicRenderer, the scene's own target read back
//             (Spout and Syphon share that texture, NDI and the capture cards
//             encode from it)
//   encoder   the scene's target fed to a score::gfx::I420Encoder, Y plane read
//             back (the libav and GStreamer YUV paths, NDI's wire encoders)
//
//   DISPLAY=:0 ctest -R gfx_offscreen_output_msaa
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_offscreen_output_msaa
#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/SinkNode.hpp>
#include <Gfx/Graph/encoders/ColorSpaceOut.hpp>
#include <Gfx/Graph/encoders/I420.hpp>
#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score_test/App.hpp>
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <memory>
#include <string>

using namespace score::test::gfx;

namespace
{
constexpr int kSize = 64;

QString edgeShader()
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-msaa-output-edge.fs");
}

struct samples_guard
{
  Gfx::Settings::Model& gfx;
  int previous{gfx.getSamples()};
  ~samples_guard() { gfx.setSamples(previous); }
};

enum class OutPath
{
  Preview,
  Sink,
  Encoder
};

const char* pathName(OutPath p)
{
  switch(p)
  {
    case OutPath::Preview:
      return "preview (BackgroundNode)";
    case OutPath::Sink:
      return "sink (SinkNode)";
    case OutPath::Encoder:
      return "encoder (I420 Y plane)";
  }
  return "?";
}

//! One channel of the delivered picture, row-major, with how it was produced.
struct Shot
{
  bool skipped{};
  std::string error;
  int stateSamples{};  //!< RenderState::samples of the output
  int targetSamples{}; //!< sample count of the target the scene is drawn into
  int width{}, height{};
  std::vector<int> values;
  //! Pixels whose channels differ: a colour where only greys can be.
  int coloured{};
};

void takeRgba(Shot& s, const QRhiReadbackResult& rb)
{
  s.width = rb.pixelSize.width();
  s.height = rb.pixelSize.height();
  if(s.width <= 0 || s.height <= 0 || rb.data.size() < qsizetype(s.width) * s.height * 4)
  {
    s.error = "empty readback";
    return;
  }
  const auto* d = reinterpret_cast<const uint8_t*>(rb.data.constData());
  const int stride = rb.data.size() / s.height;
  for(int y = 0; y < s.height; y++)
    for(int x = 0; x < s.width; x++)
    {
      const auto* p = d + std::size_t(y) * stride + std::size_t(x) * 4;
      s.values.push_back(p[0]);
      if(std::abs(p[0] - p[1]) > 8 || std::abs(p[0] - p[2]) > 8)
        s.coloured++;
    }
}

//! The encoder path: the scene drawn into createOutputRenderTarget's target,
//! converted by an I420 encoder that reads the resolved texture.
class EncoderSink final : public score::gfx::OutputNode
{
public:
  EncoderSink() { input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}}); }
  ~EncoderSink() override { destroyOutput(); }

  bool canRender() const override { return bool(m_renderState) && bool(m_target); }
  void startRendering() override { }
  void stopRendering() override { }
  void onRendererChange() override { }
  void setRenderer(std::shared_ptr<score::gfx::RenderList> r) override { m_renderer = r; }
  score::gfx::RenderList* renderer() const override { return m_renderer.lock().get(); }
  std::shared_ptr<score::gfx::RenderState> renderState() const override
  {
    return m_renderState;
  }
  score::gfx::TextureRenderTarget currentRenderTarget() const noexcept override
  {
    return m_target;
  }
  Configuration configuration() const noexcept override
  {
    return {.manualRenderingRate = 1000. / 30.};
  }

  void createOutput(score::gfx::OutputConfiguration conf) override
  {
    m_renderState
        = score::gfx::createRenderState(conf.graphicsApi, QSize(kSize, kSize), nullptr);
    if(!m_renderState || !m_renderState->rhi)
    {
      m_renderState.reset();
      return;
    }
    m_renderState->outputSize = m_renderState->renderSize;
    auto& rhi = *m_renderState->rhi;
    m_texture = rhi.newTexture(
        QRhiTexture::RGBA8, m_renderState->renderSize, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    m_texture->create();
    m_target = score::gfx::createOutputRenderTarget(
        *m_renderState, *m_texture, score::gfx::OutputDepth::DepthStencil);

    m_encoder = std::make_unique<score::gfx::I420Encoder>();
    m_encoder->init(
        rhi, *m_renderState, m_texture, kSize, kSize, score::gfx::colorMatrixOut());
    if(conf.onReady)
      conf.onReady();
  }

  void destroyOutput() override
  {
    if(m_encoder)
    {
      m_encoder->release();
      m_encoder.reset();
    }
    if(!m_renderState)
      return;
    releaseRegistry();
    score::gfx::releaseOutputRenderTarget(*m_renderState, m_target);
    delete m_texture;
    m_texture = nullptr;
    m_renderState->destroy();
    m_renderState.reset();
  }

  score::gfx::OutputNodeRenderer*
  createRenderer(score::gfx::RenderList&) const noexcept override
  {
    return new Gfx::BasicRenderer{m_target, *m_renderState, *this};
  }

  void render() override
  {
    auto renderer = m_renderer.lock();
    if(!renderer || !canRender())
      return;
    auto& rhi = *m_renderState->rhi;
    score::gfx::OffscreenFrame frame{rhi};
    if(!frame)
      return;
    renderer->render(frame.commands());
    m_encoder->exec(rhi, frame.commands());
    frame.end();
  }

  const QRhiReadbackResult& luma() const { return m_encoder->readback(0); }

private:
  std::weak_ptr<score::gfx::RenderList> m_renderer;
  std::shared_ptr<score::gfx::RenderState> m_renderState;
  QRhiTexture* m_texture{};
  score::gfx::TextureRenderTarget m_target;
  std::unique_ptr<score::gfx::GPUVideoEncoder> m_encoder;
};

//! Reads the texture an output's scene was resolved into.
QRhiReadbackResult readTexture(QRhi& rhi, QRhiTexture& tex)
{
  QRhiReadbackResult rb;
  score::gfx::OffscreenFrame frame{rhi};
  if(!frame)
    return rb;
  auto* batch = rhi.nextResourceUpdateBatch();
  batch->readBackTexture(QRhiReadbackDescription{&tex}, &rb);
  frame.commands().resourceUpdate(batch);
  frame.end();
  return rb;
}

void runPreview(Shot& s, score::gfx::GraphicsApi api)
{
  GfxPipeline p;
  const int edge = p.addIsf(edgeShader());
  const int sink = p.addSink({kSize, kSize});
  if(edge >= 0)
    p.wire(p.imageOut(edge, 0), p.sinkInput(sink));
  if(!p.create(api))
  {
    s.skipped = p.skipped();
    s.error = s.skipped ? p.skipReason() : p.error();
    return;
  }
  auto* bg = p.sink(sink);
  s.stateSamples = bg->renderState()->samples;
  s.targetSamples = bg->currentRenderTarget().sampleCount();
  p.render(4);
  s.error = p.error();
  if(s.error.empty())
    takeRgba(s, *bg->shared_readback);
}

//! The sink and encoder paths, through a graph of their own: the source, then
//! the output under test.
template <typename Output, typename Read>
void runGraph(Shot& s, score::gfx::GraphicsApi api, Output* out, Read read)
{
  auto built = make_isf_node(edgeShader());
  if(!built.node)
  {
    s.error = built.error;
    delete out;
    return;
  }
  std::string probed;
  if(api == score::gfx::Null || !probe_api(api, probed))
  {
    s.skipped = true;
    s.error = std::string("backend ") + backend_name(api) + " unavailable";
    delete out;
    return;
  }

  auto graph = std::make_unique<score::gfx::Graph>();
  graph->addNode(built.node.get());
  graph->addNode(out);
  graph->addEdge(
      built.node->output[0], out->input[0], Process::CableType::ImmediateGlutton);
  graph->createAllRenderLists(api);

  if(auto rs = out->renderState(); rs && rs->rhi && out->currentRenderTarget())
  {
    s.stateSamples = rs->samples;
    s.targetSamples = out->currentRenderTarget().sampleCount();
    for(int i = 0; i < 4; i++)
      out->render();
    read(*rs->rhi);
  }
  else
  {
    s.skipped = true;
    s.error = "the output could not create its render state";
  }

  graph.reset();
  delete out;
}

Shot render(score::gfx::GraphicsApi api, OutPath path, int samples)
{
  Shot s;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    samples_guard guard{ctx.settings<Gfx::Settings::Model>()};
    guard.gfx.setSamples(samples);

    switch(path)
    {
      case OutPath::Preview:
        runPreview(s, api);
        break;
      case OutPath::Sink: {
        auto* sink = new score::gfx::SinkNode{1000.};
        runGraph(s, api, sink, [&](QRhi& rhi) {
          if(auto* tex = sink->currentRenderTarget().texture)
            takeRgba(s, readTexture(rhi, *tex));
          else
            s.error = "no output texture";
        });
        break;
      }
      case OutPath::Encoder: {
        auto* enc = new EncoderSink;
        runGraph(s, api, enc, [&](QRhi&) {
          const auto& rb = enc->luma();
          s.width = rb.pixelSize.width();
          s.height = rb.pixelSize.height();
          if(s.width <= 0 || s.height <= 0 || rb.data.isEmpty())
          {
            s.error = "empty Y plane";
            return;
          }
          const auto* d = reinterpret_cast<const uint8_t*>(rb.data.constData());
          const int stride = rb.data.size() / s.height;
          for(int y = 0; y < s.height; y++)
            for(int x = 0; x < s.width; x++)
              s.values.push_back(d[std::size_t(y) * stride + x]);
        });
        break;
      }
    }
  });
  return s;
}

//! How the picture's values split between the two levels of the scene and the
//! steps between them, relative to its own range: the Y plane is limited range.
struct Levels
{
  int lo{}, hi{};
  int dark{}, bright{}, between{};
};

Levels levels(const Shot& s)
{
  Levels l;
  const auto [mn, mx] = std::minmax_element(s.values.begin(), s.values.end());
  l.lo = *mn;
  l.hi = *mx;
  const double span = double(l.hi - l.lo);
  for(int v : s.values)
  {
    const double t = span > 0 ? (v - l.lo) / span : 0.;
    if(t < 0.1)
      l.dark++;
    else if(t > 0.9)
      l.bright++;
    else
      l.between++;
  }
  return l;
}
}

TEST_CASE(
    "an offscreen output renders at the render list's sample count",
    "[gfx][output][msaa]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto path = GENERATE(OutPath::Preview, OutPath::Sink, OutPath::Encoder);
  const int samples = GENERATE(1, 4);
  CAPTURE(backend_name(api), pathName(path), samples);

  const auto s = render(api, path, samples);
  if(s.skipped)
  {
    WARN("skipped: " << s.error);
    return;
  }
  INFO("error=" << s.error);
  REQUIRE(s.error.empty());
  // SinkNode renders at its default size, the others at kSize.
  REQUIRE(s.width > 0);
  REQUIRE(s.height > 0);

  const auto l = levels(s);
  INFO(
      "state samples " << s.stateSamples << ", target samples " << s.targetSamples
                       << ", range " << l.lo << ".." << l.hi << ", dark " << l.dark
                       << ", bright " << l.bright << ", between " << l.between
                       << ", coloured " << s.coloured);

  // The picture itself: black around a white square of a quarter of the frame,
  // and nothing but greys. A QRhi that did not come up, or a target the
  // passes do not match, gives a flat or coloured picture instead.
  CHECK(s.coloured == 0);
  CHECK(l.hi - l.lo > 150);
  const int total = s.width * s.height;
  CHECK(l.bright > total / 6);
  CHECK(l.bright < total / 3);
  CHECK(l.dark > total / 2);

  if(samples == 1)
  {
    CHECK(s.targetSamples == 1);
    CHECK(l.between == 0);
    return;
  }

  // Not a SKIP, which would report the whole case skipped and hide the others.
  if(s.stateSamples < 2)
  {
    WARN("this backend gives the render list no multisampling");
    return;
  }
  CHECK(s.targetSamples == s.stateSamples);
  // The four edges cross about width + height pixels, most of them partly
  // covered.
  CHECK(l.between >= (s.width + s.height) / 3);
}
