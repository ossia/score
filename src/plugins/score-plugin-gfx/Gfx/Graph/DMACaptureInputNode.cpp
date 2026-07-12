#include "DMACaptureInputNode.hpp"

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/interop/VideoCaptureStrategy.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QDebug>

#include <chrono>
#include <memory>

namespace score::gfx
{

DMACaptureBackend::~DMACaptureBackend() = default;

/**
 * @brief Generic renderer for DMA capture-card inputs.
 *
 * Owns the QRhi-side machinery; the vendor `DMACaptureBackend` owns the device
 * and capture thread. Per-frame flow:
 *   capture thread (backend): DMA card -> strategy slot -> publish in slot ring
 *   render thread (here): poll ring -> acquireForRender (sysmem->texture) ->
 *                         sample via decoder -> releaseAfterRender next tick
 */
class DMACaptureInputNode::Renderer final : public score::gfx::NodeRenderer
{
public:
  explicit Renderer(const DMACaptureInputNode& n)
      : score::gfx::NodeRenderer{n}
      , node{n}
  {
  }
  ~Renderer() override
  {
    // Defensive teardown. release() is the only place that stops the capture,
    // and not every deletion path reaches it, so repeat the ordering it
    // documents: stop the vendor capture thread first (no more DMA in flight),
    // then release the strategy -- which unpins its DMA buffers *through the
    // card* -- and only then drop the backend that owns the card handle.
    // No-op when release() already ran: it nulls everything it frees.
    if(m_backend)
      m_backend->stop();
    if(m_strategy)
    {
      m_strategy->release();
      m_strategy.reset();
    }
    m_backend.reset();
  }

  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port&) override
  {
    return {};
  }

  void
  init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto& rhi = *renderer.state.rhi;
    m_backendKind = rhi.backend();

    // Must be defaultQuad(): score::gfx::quadRenderPass() hardcodes
    // renderer.defaultQuad() for the draw call (TexturedQuad::setupBindings +
    // 4-vertex draw). Building the mesh buffer or pipeline from a *triangle*
    // makes the draw read the texcoord vertex-binding at the quad's byte offset
    // (4*2*float) instead of the triangle's (3*2*float) and over-run the vertex
    // count — which garbles the sampled texcoords.
    if(m_meshBuffer.buffers.empty())
      m_meshBuffer = renderer.initMeshBuffer(renderer.defaultQuad(), res);

    m_processUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
        sizeof(score::gfx::ProcessUBO));
    m_processUBO->create();
    m_materialUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
        sizeof(score::gfx::VideoMaterialUBO));
    m_materialUBO->create();

    m_backend = node.makeCaptureBackend(m_ring);
    if(!m_backend || !m_backend->open())
    {
      qWarning() << "DMA capture: device open failed; falling back";
      m_backend.reset();
      return;
    }

    // Strategy construction must come AFTER open: vendor strategies capture the
    // device handle at construction. pickStrategy may return null (no GPU-direct
    // strategy for this backend) — the CPU-staging fallback below still applies.
    m_strategy = m_backend->pickStrategy(m_backendKind);

    // Decoder + texture. The decoder allocates an input texture sized to the
    // card's wire byte layout; the strategy DMAs raw bytes into it and the
    // decoder's shader unpacks + converts to RGB at sample time. The backend's
    // colour metadata (VPID/InfoFrame-derived) drives the conversion.
    static_cast<Video::ImageFormat&>(m_metadata) = m_backend->imageFormat();
    m_gpu = m_backend->makeDecoder(m_metadata);
    if(!m_gpu)
    {
      qWarning() << "DMA capture: no decoder for wire format; falling back";
      m_backend.reset();
      return;
    }
    auto shaders = m_gpu->init(renderer);
    m_vertexS = shaders.first;
    m_fragmentS = shaders.second;
    SCORE_ASSERT(!m_gpu->samplers.empty());

    // Strategy init must precede defaultPassesInit: a zero-copy strategy may
    // provide its own renderer-facing texture (see the swap below), which the
    // pass's shader-resource bindings must reference.
    score::gfx::interop::VideoCaptureStrategyConfig icfg{
        .rhi = &rhi,
        .state = &renderer.state,
        .frameByteSize = m_backend->frameByteSize(),
        .width = m_backend->width(),
        .height = m_backend->height(),
        .outputTexture = m_gpu->samplers[0].texture};
    if(m_strategy)
      m_backend->setStrategy(m_strategy.get());
    if(!m_strategy || !m_strategy->init(icfg))
    {
      if(m_strategy)
        qDebug() << "DMA capture: strategy" << m_strategy->name()
                 << "init failed; using CPU-staging";
      // Universal CPU-staging fallback (works on every backend).
      m_strategy = m_backend->makeCpuStrategy();
      if(m_strategy)
        m_backend->setStrategy(m_strategy.get());
      if(!m_strategy || !m_strategy->init(icfg))
      {
        qWarning() << "DMA capture: no working capture strategy; falling back";
        m_backend.reset();
        m_strategy.reset();
        return;
      }
    }

    // Some zero-copy strategies (the Vulkan zero-copy path) allocate the renderer-facing
    // texture themselves — an exportable, CUDA-mapped VkImage — instead of
    // uploading into the decoder's. When the strategy provides its own texture,
    // swap it into the decoder's sampler so the pass samples it. The strategy
    // owns that texture (frees it in release()); release() below detaches it
    // from the decoder to avoid a double-free.
    if(auto* st = m_strategy->outputTexture();
       st && st != m_gpu->samplers[0].texture)
    {
      delete m_gpu->samplers[0].texture; // decoder's original input texture
      m_gpu->samplers[0].texture = st;
      m_strategyOwnsTexture = true;
    }
    // Baseline for the per-frame rebind below: the texture the passes are about
    // to be built against. A double-buffering strategy will swap to a different
    // slot texture on later frames (currentTexture()); single-texture
    // strategies keep this forever.
    m_currentTex = m_gpu->samplers[0].texture;

    score::gfx::defaultPassesInit(
        m_p, this->node.output[0]->edges, renderer, renderer.defaultQuad(),
        shaders.first, shaders.second, m_processUBO, m_materialUBO,
        m_gpu->samplers);

    // Material UBO holds the OUTPUT (consumer-visible) texture size, always the
    // full pixel geometry regardless of whether the input texture is
    // half-width / quarter-width. Decoders' shaders use mat.texSz.x to compute
    // the output column index for chroma upsampling (UYVY) or v210 group decode.
    m_material.textureSize[0] = float(m_metadata.width);
    m_material.textureSize[1] = float(m_metadata.height);
    res.updateDynamicBuffer(
        m_materialUBO, 0, sizeof(score::gfx::VideoMaterialUBO), &m_material);

    qDebug() << "DMA capture: GPU-direct" << m_strategy->name() << "engaged"
             << m_backend->width() << "x" << m_backend->height();

    m_backend->start();
    // Baseline the live-format channel to what we just opened at, so the first
    // update() doesn't mistake the initial publish for a change.
    m_lastFormatGen = m_ring.loadFormat().generation;
  }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge*) override
  {
    // Live input-resolution change (Spout-style consumer side): the capture
    // thread published a new wire geometry into the ring. Rebuild our
    // size-dependent GPU resources at the new size with the same teardown+reinit
    // the RenderList runs on a window resize. release() stops the backend first,
    // so the capture thread cannot write a new-size frame into an old-size slot.
    // Done at the top of update(), before this frame draws with m_p, so the
    // rebuilt passes are the ones used. init() re-opens the backend, which for
    // an auto-detecting input re-reads the current wire format.
    if(const auto fmt = m_ring.loadFormat();
       fmt.generation != m_lastFormatGen && fmt.width > 0 && fmt.height > 0)
    {
      qDebug() << "DMA capture: input format changed to" << fmt.width << "x"
               << fmt.height << "- reallocating";
      release(renderer);
      init(renderer, res);
      return;
    }

    if(!m_strategy)
      return;
    res.updateDynamicBuffer(
        m_processUBO, 0, sizeof(score::gfx::ProcessUBO),
        &this->node.standardUBO);

    // Poll the capture thread's frame counter. If it advanced since we last
    // sampled, hand the previous frame's texture back to the strategy and take
    // ownership of the new one. Doing the release on the NEXT update() — rather
    // than a per-edge finishFrame hook — works because by the time we're back in
    // update() the QRhi command buffer for the previous frame has been
    // submitted, so the EndAPI signal sequences correctly with the WaitDVP in
    // the capture thread's next ingestFrame.
    const uint64_t latest = m_ring.latestFrameId.load(std::memory_order_acquire);
    if(latest != m_lastIngestedFrameId)
    {
      if(m_renderHoldsTexture)
      {
        m_strategy->releaseAfterRender();
        m_renderHoldsTexture = false;
      }
      // Pass the active resource-update batch: the portable CPU strategy uses it
      // for QRhi uploadTexture; raw-API strategies (GL/DVP) ignore it and fall
      // through to the no-arg acquireForRender().
      if(m_timeUpload) [[unlikely]]
      {
        const auto t0 = std::chrono::steady_clock::now();
        m_strategy->acquireForRender(res);
        m_uploadTotalNs += std::chrono::duration_cast<std::chrono::nanoseconds>(
                               std::chrono::steady_clock::now() - t0)
                               .count();
        ++m_uploadCount;
      }
      else
      {
        m_strategy->acquireForRender(res);
      }
      m_lastIngestedFrameId = latest;
      m_renderHoldsTexture = true;

      // Double-buffered strategies (the Vulkan zero-copy path) publish a fresh sampled
      // texture per frame so the capture-thread write and the render-thread
      // sample never touch the same VkImage. When the current texture changes,
      // rebind every pass's SRB to it (updateResources only — no pipeline
      // rebuild). No-op for single-texture strategies: their currentTexture()
      // is constant, so cur == m_currentTex every frame.
      if(m_gpu && !m_gpu->samplers.empty())
      {
        if(auto* cur = m_strategy->currentTexture(); cur && cur != m_currentTex)
        {
          QRhiSampler* s = m_gpu->samplers[0].sampler;
          for(auto& pass : m_p)
            if(pass.second.p.srb)
              score::gfx::replaceTexture(*pass.second.p.srb, s, cur);
          m_gpu->samplers[0].texture = cur;
          m_currentTex = cur;
        }
      }
    }
  }

  void runRenderPass(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      score::gfx::Edge& edge) override
  {
    score::gfx::quadRenderPass(renderer, m_meshBuffer, cb, edge, m_p);
  }
  void addOutputPass(
      score::gfx::RenderList& renderer, score::gfx::Edge& edge,
      QRhiResourceUpdateBatch& res) override
  {
    if(!m_gpu || m_gpu->samplers.empty() || !m_vertexS.isValid()
       || !m_fragmentS.isValid())
      return;

    auto rt = renderer.renderTargetForOutput(edge);
    if(rt.renderTarget)
    {
      auto pip = score::gfx::buildPipeline(
          renderer, renderer.defaultQuad(), m_vertexS, m_fragmentS, rt,
          m_processUBO, m_materialUBO, m_gpu->samplers);
      if(pip.pipeline)
        m_p.emplace_back(&edge, score::gfx::Pass{rt, pip, nullptr});
    }
  }

  void removeOutputPass(
      score::gfx::RenderList& renderer, score::gfx::Edge& edge) override
  {
    auto it = ossia::find_if(m_p, [&](const auto& p) { return p.first == &edge; });
    if(it != m_p.end())
    {
      it->second.release();
      m_p.erase(it);
    }
  }

  bool hasOutputPassForEdge(score::gfx::Edge& edge) const override
  {
    // Without this, Graph::createAllMissingPasses (re-run on every
    // document edit) can't tell this edge already has a pass and stacks
    // duplicates; removeOutputPass then drops only the first, leaving a
    // stale pass bound to a destroyed render target — the UAF class
    // addOutputPass/removeOutputPass exist to prevent.
    return ossia::find_if(m_p, [&](const auto& p) { return p.first == &edge; })
           != m_p.end();
  }

  void release(score::gfx::RenderList& r) override
  {
    if(m_timeUpload && m_uploadCount > 0) [[unlikely]]
    {
      qDebug().nospace()
          << "DMA capture: upload(" << (m_strategy ? m_strategy->name() : "?")
          << ") mean " << (double(m_uploadTotalNs) / m_uploadCount / 1000.0)
          << " us over " << m_uploadCount << " frames";
    }
    // If the strategy owns the decoder's sampler texture (the Vulkan zero-copy swap,
    // recorded at init), detach it before either release() runs so the decoder
    // doesn't free a texture the strategy also frees. For every other strategy
    // outputTexture() IS the decoder's own texture — detaching would leak it,
    // since GPUVideoDecoder::release() is what deletes it.
    if(m_strategyOwnsTexture && m_gpu && !m_gpu->samplers.empty())
      m_gpu->samplers[0].texture = nullptr;
    m_strategyOwnsTexture = false;
    if(m_strategy && m_renderHoldsTexture)
    {
      m_strategy->releaseAfterRender();
      m_renderHoldsTexture = false;
    }
    // Stop the vendor capture thread first (no more DMA in flight), then
    // release the strategy — which unpins its DMA buffers *through the card*
    // — and only afterwards destroy the backend that owns the card handle.
    // Resetting the backend before the strategy would close the card out from
    // under DMABufferUnlock (a use-after-free that only surfaces once a
    // real GPUDirect-RDMA capture strategy actually pinned buffers).
    if(m_backend)
      m_backend->stop();
    if(m_strategy)
    {
      m_strategy->release();
      m_strategy.reset();
    }
    if(m_backend)
      m_backend.reset();
    for(auto& p : m_p)
      p.second.release();
    m_p.clear();
    m_meshBuffer = {}; // Freed in RenderList
    delete m_processUBO;
    m_processUBO = nullptr;
    delete m_materialUBO;
    m_materialUBO = nullptr;
    if(m_gpu)
    {
      m_gpu->release(r);
      m_gpu.reset();
    }
  }

private:
  const DMACaptureInputNode& node;
  QRhi::Implementation m_backendKind{QRhi::Null};

  std::unique_ptr<DMACaptureBackend> m_backend;
  std::unique_ptr<score::gfx::interop::VideoCaptureStrategy> m_strategy;
  score::gfx::interop::VideoCaptureSlotRing m_ring;
  uint64_t m_lastIngestedFrameId{0};
  uint64_t m_lastFormatGen{0};
  bool m_renderHoldsTexture{false};
  /// True when the strategy swapped its own texture into the decoder sampler
  /// (the Vulkan zero-copy path); gates the detach in release() so decoder-owned textures
  /// are still freed by GPUVideoDecoder::release().
  bool m_strategyOwnsTexture{false};
  /// The texture currently bound in every pass's SRB. Tracked so a
  /// double-buffering strategy's per-frame texture change triggers exactly one
  /// SRB rebind; constant for single-texture strategies.
  QRhiTexture* m_currentTex{};

  std::unique_ptr<score::gfx::GPUVideoDecoder> m_gpu;
  QShader m_vertexS, m_fragmentS;
  score::gfx::PassMap m_p;
  score::gfx::MeshBuffers m_meshBuffer{};
  QRhiBuffer* m_processUBO{};
  QRhiBuffer* m_materialUBO{};
  score::gfx::VideoMaterialUBO m_material;
  Video::VideoMetadata m_metadata;

  // Diagnostic: SCORE_DMACAPTURE_TIME_UPLOAD=1 times the per-frame
  // acquireForRender (the sysmem -> texture upload) to compare raw-API vs
  // portable-QRhi cost.
  const bool m_timeUpload
      = qEnvironmentVariableIsSet("SCORE_DMACAPTURE_TIME_UPLOAD");
  uint64_t m_uploadTotalNs{0};
  uint64_t m_uploadCount{0};
};

DMACaptureInputNode::DMACaptureInputNode()
{
  output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
}

DMACaptureInputNode::~DMACaptureInputNode() = default;

NodeRenderer*
DMACaptureInputNode::createRenderer(RenderList& r) const noexcept
{
  return new Renderer{*this};
}

} // namespace score::gfx
