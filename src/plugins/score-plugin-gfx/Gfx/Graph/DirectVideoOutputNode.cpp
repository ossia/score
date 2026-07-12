#include "DirectVideoOutputNode.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/encoders/WireEncoderFactory.hpp>
#include <Gfx/Graph/interop/VideoOutputStrategy.hpp>
#include <Gfx/Graph/interop/VideoOutputStrategySelect.hpp>
#include <Gfx/Graph/interop/CpuStagedVideoOutput.hpp>
#include <Gfx/Graph/interop/PacedFramePump.hpp>
#include <Gfx/InvertYRenderer.hpp>

#include <QDebug>

namespace score::gfx
{

DirectVideoOutputNode::DirectVideoOutputNode(
    std::unique_ptr<DirectVideoOutputBackend> backend)
    : m_backend{std::move(backend)}
{
  // Image input port - upstream nodes render into m_texture.
  input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
}

DirectVideoOutputNode::~DirectVideoOutputNode()
{
  stopRendering();
  destroyOutput();
}

void DirectVideoOutputNode::setRenderer(std::shared_ptr<RenderList> r)
{
  m_renderer = r;
}

RenderList* DirectVideoOutputNode::renderer() const
{
  return m_renderer.lock().get();
}

OutputNodeRenderer*
DirectVideoOutputNode::createRenderer(RenderList& r) const noexcept
{
  // The encoder reads m_texture directly (no Y-flip / readback here; the encoder
  // shader handles both cross-backend), so BasicRenderer is the right fit.
  score::gfx::TextureRenderTarget rt{
      .texture = m_texture,
      .renderPass = m_renderState->renderPassDescriptor,
      .renderTarget = m_renderTarget};
  return new Gfx::BasicRenderer{rt, *m_renderState, *this};
}

void DirectVideoOutputNode::startRendering()
{
  m_running = true;
  if(m_pump)
    m_pump->start();
}

void DirectVideoOutputNode::stopRendering()
{
  m_running = false;
  if(m_pump)
    m_pump->stop();
}

bool DirectVideoOutputNode::canRender() const
{
  if(!(m_backend && m_backend->isOpen() && m_renderState && m_texture))
    return false;
  if(m_rdma)
    return true;
  return m_hostStaged && m_hostStaged->valid();
}

void DirectVideoOutputNode::onRendererChange() { }

DirectVideoOutputNode::Configuration
DirectVideoOutputNode::configuration() const noexcept
{
  Configuration conf;
  conf.manualRenderingRate = 1000.0 / m_backend->frameRate();
  conf.outputNeedsRenderPass = false;
  conf.supportsVSync = false;
  return conf;
}

std::shared_ptr<RenderState> DirectVideoOutputNode::renderState() const
{
  return m_renderState;
}

const char* DirectVideoOutputNode::activeStrategyName() const noexcept
{
  return m_rdma ? m_rdma->name() : "cpu-staging";
}

std::function<bool()> DirectVideoOutputNode::genlockTickSource() const
{
  return m_backend ? m_backend->genlockTickSource() : std::function<bool()>{};
}

std::uint64_t DirectVideoOutputNode::pacingGoodXfers() const noexcept
{
  return m_pump ? m_pump->goodXfers() : 0;
}
std::uint64_t DirectVideoOutputNode::pacingDrops() const noexcept
{
  return m_pump ? m_pump->drops() : 0;
}
std::uint64_t DirectVideoOutputNode::pacingUnderruns() const noexcept
{
  return m_pump ? m_pump->underruns() : 0;
}

void DirectVideoOutputNode::createOutput(OutputConfiguration conf)
{
  m_graphicsApi = conf.graphicsApi;

  if(!m_backend->open(conf.graphicsApi))
  {
    qWarning() << "DirectVideoOutput: failed to open device";
    return;
  }

  const QSize size{m_backend->width(), m_backend->height()};
  m_renderState = score::gfx::createRenderState(conf.graphicsApi, size, nullptr);
  if(!m_renderState || !m_renderState->rhi)
  {
    qWarning() << "DirectVideoOutput: failed to create render state";
    m_backend->close();
    return;
  }

  m_renderState->renderSize = size;
  m_renderState->outputSize = size;
  m_renderState->api = conf.graphicsApi;

  // RGBA16F intermediate for >8-bit / HDR wire formats; plain 8-bit SDR -> RGBA8.
  m_renderState->renderFormat
      = m_backend->prefersFloatRender() ? QRhiTexture::RGBA16F
                                        : QRhiTexture::RGBA8;

  m_rhi = m_renderState->rhi;

  // Probe GPU interop once (borrowed by CpuStagedVideoOutput's DVP ring below).
  m_caps = interop::probeContextFree();
  interop::probeFromQRhi(m_caps, m_rhi);

  // VBI-paced submit pump (backend hooks wait on the output tick and submit the
  // frame off the render thread).
  m_pump = std::make_unique<interop::PacedFramePump>(
      m_backend->pacingHooks(), /*ringDepth=*/3);

  m_texture = m_rhi->newTexture(
      m_renderState->renderFormat, size, 1,
      QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
  if(!m_texture->create())
  {
    qWarning() << "DirectVideoOutput: failed to create render texture";
    destroyOutput();
    return;
  }

  m_renderTarget = m_rhi->newTextureRenderTarget({m_texture});
  m_renderState->renderPassDescriptor
      = m_renderTarget->newCompatibleRenderPassDescriptor();
  m_renderTarget->setRenderPassDescriptor(m_renderState->renderPassDescriptor);
  if(!m_renderTarget->create())
  {
    qWarning() << "DirectVideoOutput: failed to create render target";
    destroyOutput();
    return;
  }

  // GPU-direct fast path: the backend supplies the per-API strategy candidates;
  // the generic picker keeps the first whose init() succeeds.
  auto candidates = m_backend->gpuDirectCandidates(m_rhi, conf.graphicsApi);
  if(!candidates.empty())
  {
    interop::VideoOutputStrategyConfig rcfg{
        .rhi = m_rhi,
        .state = m_renderState.get(),
        .sourceTexture = m_texture,
        .width = m_backend->width(),
        .height = m_backend->height(),
        .frameByteSize = m_backend->frameByteSize()};
    m_rdma = interop::selectVideoOutputStrategy(
        rcfg, std::move(candidates),
        [](const char* n) {
          qDebug().noquote() << QStringLiteral(
              "DirectVideoOutput: %1 engaged - GPU->card, no CPU staging")
              .arg(QString::fromUtf8(n));
        },
        [](const char* n) {
          qWarning().noquote() << QStringLiteral(
              "DirectVideoOutput: %1 init failed - next path")
              .arg(QString::fromUtf8(n));
        });
    if(m_rdma)
    {
      if(conf.onReady)
        conf.onReady();
      return;
    }
  }

  // -------- CPU staging path --------
  auto makeEncoder = [this] {
    return score::gfx::makeWireEncoder(m_backend->encoderFormat());
  };
  auto enc0 = makeEncoder();
  auto enc1 = makeEncoder();
  if(!enc0 || !enc1)
  {
    qWarning() << "DirectVideoOutput: wire format has no GPU encoder";
    destroyOutput();
    return;
  }

  const QString colorShader = m_backend->colorConversion();
  enc0->init(
      *m_rhi, *m_renderState, m_texture, m_backend->width(), m_backend->height(),
      colorShader);
  enc1->init(
      *m_rhi, *m_renderState, m_texture, m_backend->width(), m_backend->height(),
      colorShader);

  interop::CpuStagedVideoOutputConfig hcfg;
  hcfg.rhi = m_rhi;
  hcfg.state = m_renderState.get();
  hcfg.width = m_backend->width();
  hcfg.height = m_backend->height();
  hcfg.frameByteSize = m_backend->frameByteSize();
  hcfg.visibleRows = m_backend->visibleRows();
  hcfg.slotCount = 4;
  hcfg.directDmaEnabled = !qEnvironmentVariableIsSet("SCORE_AJA_NO_DIRECT_DMA");
  hcfg.planes = m_backend->planes();
  hcfg.registrar = m_backend->registrar();
  hcfg.customStage = m_backend->customStage();
  hcfg.preferGpuDownload = m_backend->prefersGpuDownload();
  hcfg.caps = &m_caps;

  m_hostStaged = std::make_unique<interop::CpuStagedVideoOutput>();
  if(!m_hostStaged->init(std::move(hcfg), std::move(enc0), std::move(enc1)))
  {
    qWarning() << "DirectVideoOutput: host-staged output init failed";
    destroyOutput();
    return;
  }

  if(conf.onReady)
    conf.onReady();
}

void DirectVideoOutputNode::destroyOutput()
{
  // Stop the pump first so the card is idle before the backend closes it.
  if(m_pump)
  {
    m_pump->stop();
    m_pump.reset();
  }
  // Let the backend stop streaming and wait out any frames its hardware still
  // holds (DeckLink's scheduled queue, Deltacast's registered RDMA slots)
  // while the buffers below are still alive. Must precede the strategy/ring
  // teardown or the card DMA-reads freed memory.
  if(m_backend)
    m_backend->quiesce();
  if(m_rdma)
  {
    m_rdma->release();
    m_rdma.reset();
  }
  // Unpins the ring + readback buffers and releases the encoders; must run
  // before the device is closed (it unlocks via the card).
  if(m_hostStaged)
  {
    m_hostStaged->release();
    m_hostStaged.reset();
  }
  if(m_renderState)
  {
    // Persist-across-rebuild contract (OutputNode.hpp): the registry
    // outlives the RenderList, so its QRhi resources must be torn down
    // here BEFORE RenderState::destroy() frees the device — otherwise
    // they leak every teardown and re-create asserts boundRhi()==&rhi.
    releaseRegistry();

    delete m_renderTarget;
    m_renderTarget = nullptr;
    delete m_renderState->renderPassDescriptor;
    m_renderState->renderPassDescriptor = nullptr;
    delete m_texture;
    m_texture = nullptr;
    m_renderState->destroy();
    m_renderState.reset();
  }
  m_rhi = nullptr;

  if(m_backend)
    m_backend->close();
}

void DirectVideoOutputNode::render()
{
  if(!m_running || !canRender())
    return;

  auto renderer = m_renderer.lock();
  if(!renderer)
    return;

  QRhiCommandBuffer* cb = nullptr;
  if(m_rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
    return;

  // Input pipeline writes RGBA into m_texture (BasicRenderer's render target).
  renderer->render(*cb);

  if(m_rdma)
  {
    m_rdma->encodeFrame(*cb);
    m_rhi->endOffscreenFrame();
    void* gpuPtr = m_rdma->prepareNextFrame();
    if(gpuPtr && m_pump)
      m_pump->push(gpuPtr);
    return;
  }

  // CPU staging: endOffscreenFrame() is synchronous offscreen, so the readback
  // is complete on return; prepareNextFrame() stages and returns the host ptr.
  m_hostStaged->encodeFrame(*cb);
  m_rhi->endOffscreenFrame();
  if(void* p = m_hostStaged->prepareNextFrame(); p && m_pump)
    m_pump->push(p);
}

} // namespace score::gfx
