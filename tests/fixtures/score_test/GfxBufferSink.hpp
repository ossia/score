#pragma once

// =============================================================================
// L3 GPU buffer-readback sink for score's gfx render engine (design "A" from
// SPEC-SCENE-RENDER-TESTS.md §3.4 item 1).
// =============================================================================
//
// WHY THIS EXISTS
//
// A geometry- or storage-buffer-producing CSF writes into SSBOs exposed
// through Types::Geometry / Types::Buffer OUTPUT ports rather than a texture.
// The engine only dispatches a node's compute passes when the node is
// reachable from an OutputNode sink: RenderList::render walks each node's
// input ports and, for a Geometry/Buffer/Scene input with edges, runs the
// upstream renderer's update() + runInitialPasses() (RenderList.cpp, the
// `input->type == Types::Buffer || Geometry || Scene` branch around line
// 1471) — runInitialPasses is where RenderedCSFNode dispatches its compute
// passes and pushes its output geometry downstream. The only headless-
// constructible sink in <score_test/Gfx.hpp>, BackgroundNode, consumes a
// Types::Image, so a geometry-only CSF was never placed in a RenderList and
// tests/gfx/CsfGeometry.cpp had to SKIP
// (csf_geometry_readback_skip_reason()).
//
// BufferSinkNode is an OutputNode with ONE Types::Geometry input that accepts
// BOTH geometry and storage-buffer edges. Being an OutputNode, the Graph
// builds a RenderList for it (Graph::createAllRenderLists dynamic_casts every
// node added to the graph, so GfxPipeline::addNode is enough to register it),
// and the reachability walk (Graph.cpp graphwalk) follows input edges
// regardless of port type, so a geometry-only CSF chain becomes reachable and
// dispatches.
//
// WHY A SINGLE INPUT PORT: in RenderList::render's Geometry/Buffer branch the
// per-node resource-update batch is only re-armed after a port when
// `node != &output`. On the OUTPUT node itself the batch is left null once
// the first wired port has been processed, and a SECOND wired port's
// prepare_render would dereference that null batch (upstream
// `update(*this, *updateBatch, edge)`). Graph::addEdge does not type-check
// ports and the branch is selected by the SINK port's type, so one
// Geometry-typed port safely carries the storage-buffer edges too; the
// renderer tells them apart by the SOURCE port's type.
//
// The renderer harvests bytes in NodeRenderer::inputAboutToFinish, which the
// render loop calls for Geometry/Buffer inputs right AFTER the upstream
// runInitialPasses of the same frame:
//   * geometry edges: RenderedCSFNode::pushOutputGeometry delivered an
//     ossia::geometry_spec to this renderer via NodeRenderer::process(port,
//     geometry_spec, source) during runInitialPasses; its mesh buffers carry
//     ossia::geometry::gpu_buffer{handle, byte_size} entries whose handle is
//     the live QRhiBuffer*. Each attribute SSBO and each auxiliary buffer
//     riding the geometry is read back with
//     QRhiResourceUpdateBatch::readBackBuffer.
//   * storage-buffer edges: RenderList::bufferForInput(edge) resolves the
//     SOURCE renderer's virtual bufferForOutput(port)
//     (RenderedCSFNode::bufferForOutput returns its m_outStorageBuffers
//     entry) — again a plain readBackBuffer.
//
// The readbacks complete at QRhi::endOffscreenFrame inside
// BufferSinkNode::render() (QRhi guarantees enqueued readbacks are ready when
// endOffscreenFrame returns), so after render() the harvested QByteArrays are
// the EXACT bytes the compute shader wrote this frame — no QRhi::finish()
// needed, the same completion model the texture-readback sink relies on.
//
// SYMBOL VISIBILITY: gfx tests link the real score_plugin_gfx and only see
// SCORE_PLUGIN_GFX_EXPORT'ed symbols. Everything referenced here is exported
// or header-only: Node / OutputNode / OutputNodeRenderer / NodeRenderer /
// RenderList / Graph are exported classes, createRenderState is an exported
// free function, Port / Edge / BufferView / TextureRenderTarget are header-
// only structs, and ossia::geometry is a header-only value type.
// RenderedCSFNode (NOT exported) is never named: its buffers are reached
// through the exported virtual NodeRenderer::bufferForOutput (via
// RenderList::bufferForInput) and through the geometry_spec it pushes into
// this renderer's base-class storage.
//
// BUFFER LIFETIME: the CSF's attribute/storage SSBOs are created in its
// renderer's update() and released through RenderList::releaseBuffer, whose
// deleteLater semantics defer destruction to a frame boundary — a handle
// obtained after this frame's update() therefore stays valid through the
// readback enqueued in the same frame. Handles are never cached across
// frames here; every frame re-resolves them.
//
// SKIP SEMANTICS mirror Gfx.hpp: skipped=true when the backend cannot
// initialize (probe_api), when the offscreen QRhi cannot come up headless,
// or when the device lacks QRhi::Compute / QRhi::ReadBackNonUniformBuffer
// (the feature gating readbacks of Static StorageBuffers; the same guard the
// engine's indirect-draw readback in RenderedRawRasterPipelineNode and
// tests/threedim/ExtractComputeSrb.cpp use). An EMPTY readback on a backend
// that claims support is reported through `error`, never through a skip —
// per the house rule in tests/integration/WiredCases.hpp.
//
// Header-only, Catch2-free: everything reports through returned data.
// =============================================================================

#include <score_test/Gfx.hpp>

#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/Mesh.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/Uniforms.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QDebug>
#include <QSize>
#include <QString>

#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace score::test::gfx
{

// -----------------------------------------------------------------------------
// The sink node + its renderer.
// -----------------------------------------------------------------------------
class BufferSinkNode final : public score::gfx::OutputNode
{
public:
  // One named GPU-buffer readback. `rb.data` is filled when the offscreen
  // frame that enqueued it ends (BufferSinkNode::render()).
  struct NamedReadback
  {
    std::string name;
    // NOT QRhiReadbackResult. The buffer-readback result is its own type in the
    // Qt CI builds against (6.4.2 -- the failing Coverage log names
    // .../QtGui/6.4.2/QtGui/private/qrhi_p.h) and only later folds into
    // QRhiReadbackResult; Gfx/Graph/RenderState.hpp already carries the
    // project's compatibility alias for the newer Qt, and
    // EncoderMatrixTest.cpp / ExtractComputeSrb.cpp already spell it this way.
    // Writing QRhiReadbackResult here made the three readBackBuffer calls below
    // fail to compile on 6.4 (the parameter is QRhiBufferReadbackResult*).
    QRhiBufferReadbackResult rb;
  };

  // Everything the renderer harvested on the LAST rendered frame. Held
  // through a shared_ptr so createRenderer() (const, like BackgroundNode's)
  // can hand the renderer access without const_cast, and so the results
  // survive renderer churn (RenderList rebuilds).
  //
  // std::deque, not vector: readBackBuffer stores the ADDRESS of each
  // NamedReadback::rb until the frame ends, and entries are appended while
  // earlier addresses are already registered — deque never relocates on
  // push_back.
  struct Harvest
  {
    // From geometry edges: one entry per mesh attribute (named by
    // ossia::geometry::display_name — "position", "color", ...; CSF SoA
    // output puts each attribute in its own SSBO, read in full) and one per
    // auxiliary buffer riding the geometry (named by its RESOURCES NAME —
    // this is where a CSF's standalone storage buffers also surface, see
    // RenderedCSFNode::pushOutputGeometry).
    std::deque<NamedReadback> attributes;
    std::deque<NamedReadback> auxiliaries;

    // From storage-buffer edges: one entry per incoming Buffer-sourced edge,
    // in wiring order, named "storage:<source-output-port-index>".
    std::deque<NamedReadback> storage;

    int vertices{0};
    int instances{0};

    // True once a geometry_spec with a non-empty mesh reached the renderer —
    // i.e. the upstream compute pass actually dispatched and pushed. Stays
    // false when the dispatch never happened (the exact regression this sink
    // exists to catch).
    bool geometry_seen{false};

    // Set when the device lacks ReadBackNonUniformBuffer: nothing was (or
    // could be) enqueued. Drivers turn this into a SKIP.
    bool readback_unsupported{false};
  };

  std::shared_ptr<Harvest> harvest = std::make_shared<Harvest>();

  BufferSinkNode()
  {
    // Single input port; see the header comment for why the storage-buffer
    // edges share it. Port objects are owned (and deleted) by ~Node.
    input.push_back(
        new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});

    // No render pass of our own and no vsync; tests call render() manually.
    // (Unlike BackgroundNode we do not consult the Gfx settings model — a
    // fixed rate keeps construction free of app-context requirements.)
    m_conf = {.manualRenderingRate = 1000. / 60.};
  }

  ~BufferSinkNode() override { destroyOutput(); }

  /// The one input port. Wire the producer's Geometry output AND/OR any
  /// number of its Buffer outputs here.
  score::gfx::Port* sinkInput() const noexcept { return input[0]; }
  /// Aliases so wiring code reads by intent.
  score::gfx::Port* geometryInput() const noexcept { return input[0]; }
  score::gfx::Port* bufferInput() const noexcept { return input[0]; }

  /// Offscreen state size. Only meaningful BEFORE createOutput (i.e. before
  /// GfxPipeline::create / Graph::createAllRenderLists) — this sink never
  /// resizes live, no render pass depends on it.
  void setSize(QSize s) noexcept { m_size = s; }

  void startRendering() override { }
  void stopRendering() override { }
  bool canRender() const override { return true; }
  void onRendererChange() override { }

  // One offscreen frame: runs the whole RenderList (upstream update +
  // runInitialPasses => compute dispatch => our inputAboutToFinish readback
  // enqueues), then endOffscreenFrame completes the readbacks. Mirrors
  // BackgroundNode::render, including the renderers.size() > 1 gate (a
  // RenderList holding only our own renderer has nothing to dispatch).
  void render() override
  {
    auto renderer = m_renderer.lock();
    if(renderer && m_renderState)
    {
      if(renderer->renderers.size() > 1)
      {
        auto rhi = m_renderState->rhi;
        QRhiCommandBuffer* cb{};
        if(rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
          return;

        renderer->render(*cb);
        rhi->endOffscreenFrame();
      }
      else
      {
        harvest->attributes.clear();
        harvest->auxiliaries.clear();
        harvest->storage.clear();
        harvest->geometry_seen = false;
      }
    }
  }

  void setRenderer(std::shared_ptr<score::gfx::RenderList> r) override
  {
    m_renderer = r;
  }
  score::gfx::RenderList* renderer() const override
  {
    return m_renderer.lock().get();
  }

  // Mirror BackgroundNode::createOutput. We do allocate a small color+depth
  // render target even though no graphics pass ever draws into it: parts of
  // the engine treat RenderState::renderPassDescriptor / an offscreen target
  // as always-present on an output, and the cost is one 64x64 texture.
  // renderSize also seeds sizes for CSF image resources with expression-free
  // WIDTH/HEIGHT.
  void createOutput(score::gfx::OutputConfiguration conf) override
  {
    m_onReleaseRenderList = conf.onReleaseRenderList;

    m_renderState
        = score::gfx::createRenderState(conf.graphicsApi, m_size, nullptr);
    if(!m_renderState || !m_renderState->rhi)
    {
      qWarning() << "BufferSinkNode: failed to create QRhi";
      m_renderState.reset();
      return;
    }
    m_renderState->outputSize = m_renderState->renderSize;
    m_renderState->renderFormat = QRhiTexture::RGBA8;

    auto rhi = m_renderState->rhi;
    m_texture = rhi->newTexture(
        m_renderState->renderFormat, m_renderState->renderSize, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    m_texture->setName("BufferSinkNode::m_texture");
    m_texture->create();

    // D32F per the reverse-Z project rule (see BackgroundNode).
    m_depthTexture = rhi->newTexture(
        QRhiTexture::D32F, m_renderState->renderSize, 1,
        QRhiTexture::RenderTarget);
    m_depthTexture->setName("BufferSinkNode::m_depthTexture");
    m_depthTexture->create();

    QRhiTextureRenderTargetDescription desc;
    desc.setColorAttachments({QRhiColorAttachment(m_texture)});
    desc.setDepthTexture(m_depthTexture);
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
    if(m_renderState)
    {
      if(m_renderState->rhi)
      {
        // Must not be called inside a frame; drain before teardown
        // (same rationale as BackgroundNode / ScreenNode::destroyOutput).
        SCORE_ASSERT(!m_renderState->rhi->isRecordingFrame());
        m_renderState->rhi->finish();
      }

      // Registry resources must go while the QRhi is still alive
      // (persist-across-rebuild contract, see OutputNode::releaseRegistry).
      releaseRegistry();

      delete m_renderTarget;
      m_renderTarget = nullptr;

      delete m_depthTexture;
      m_depthTexture = nullptr;

      delete m_texture;
      m_texture = nullptr;

      delete m_renderState->renderPassDescriptor;
      m_renderState->renderPassDescriptor = nullptr;

      m_renderState->destroy();
      m_renderState.reset();
    }
  }

  void updateGraphicsAPI(score::gfx::GraphicsApi api) override
  {
    if(!m_renderState)
      return;
    if(m_renderState->api != api)
      destroyOutput();
  }

  std::shared_ptr<score::gfx::RenderState> renderState() const override
  {
    return m_renderState;
  }

  score::gfx::TextureRenderTarget currentRenderTarget() const noexcept override
  {
    if(!m_renderState)
      return {};
    return score::gfx::TextureRenderTarget{
        .texture = m_texture,
        .renderPass = m_renderState->renderPassDescriptor,
        .renderTarget = m_renderTarget,
        .depthTexture = m_depthTexture};
  }

  score::gfx::OutputNodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override;

  Configuration configuration() const noexcept override { return m_conf; }

private:
  Configuration m_conf;

  std::weak_ptr<score::gfx::RenderList> m_renderer{};
  QRhiTexture* m_texture{};
  QRhiTexture* m_depthTexture{};
  QRhiTextureRenderTarget* m_renderTarget{};
  std::shared_ptr<score::gfx::RenderState> m_renderState{};

  QSize m_size{64, 64};
};

class BufferSinkRenderer final : public score::gfx::OutputNodeRenderer
{
public:
  BufferSinkRenderer(
      const BufferSinkNode& node,
      std::shared_ptr<BufferSinkNode::Harvest> harvest) noexcept
      : score::gfx::OutputNodeRenderer{node}
      , m_harvest{std::move(harvest)}
  {
  }

  // No GPU state of our own: the harvested buffers belong to the upstream
  // renderers, and the readback results live on the CPU side.
  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override { }
  void update(
      score::gfx::RenderList&, QRhiResourceUpdateBatch&,
      score::gfx::Edge*) override
  {
  }
  void release(score::gfx::RenderList&) override { }

  // Called by RenderList::render for our Geometry input port, AFTER the
  // upstream nodes' runInitialPasses dispatched this frame's compute passes
  // (and, for geometry, pushed their geometry_spec into this renderer).
  // `res` may be null here — the loop flushed it right before the call and
  // submits whatever we leave in it right after, still inside the offscreen
  // frame, so the readbacks complete at endOffscreenFrame.
  void inputAboutToFinish(
      score::gfx::RenderList& renderer, const score::gfx::Port& p,
      QRhiResourceUpdateBatch*& res) override
  {
    QRhi& rhi = *renderer.state.rhi;
    if(!rhi.isFeatureSupported(QRhi::ReadBackNonUniformBuffer))
    {
      // Static StorageBuffer readbacks need this feature (the CSF SSBOs are
      // created Static | StorageBuffer | VertexBuffer). Drivers turn the
      // flag into a SKIP; enqueuing anyway would be invalid on e.g. GLES2.
      m_harvest->readback_unsupported = true;
      return;
    }

    const auto ensureBatch = [&]() -> QRhiResourceUpdateBatch* {
      if(!res)
        res = rhi.nextResourceUpdateBatch();
      return res;
    };

    // Which of our input ports is this? (There is one today; stay general.)
    int portIndex = -1;
    for(std::size_t i = 0; i < node.input.size(); ++i)
      if(node.input[i] == &p)
      {
        portIndex = int(i);
        break;
      }
    if(portIndex < 0)
      return;

    // Fresh harvest for this frame. Safe to clear: last frame's readbacks
    // completed at that frame's endOffscreenFrame, so no QRhi-held pointer
    // into these deques is outstanding.
    m_harvest->attributes.clear();
    m_harvest->auxiliaries.clear();
    m_harvest->storage.clear();
    m_harvest->geometry_seen = false;

    // --- Geometry: the spec the CSF pushed this frame (base-class per-port
    // storage, filled by NodeRenderer::process(port, geometry_spec, source)
    // from RenderedCSFNode::pushOutputGeometry).
    if(const ossia::geometry_spec* spec = findGeometryByPort(portIndex);
       spec && spec->meshes && !spec->meshes->meshes.empty())
    {
      const ossia::geometry& mesh = spec->meshes->meshes[0];
      m_harvest->geometry_seen = true;
      m_harvest->vertices = mesh.vertices;
      m_harvest->instances = mesh.instances;

      // Attribute SSBOs: attr.binding indexes the parallel bindings/input
      // arrays; input[binding].buffer indexes buffers (the same indirection
      // RenderedCSFNode::updateStorageBuffers walks).
      for(const ossia::geometry::attribute& attr : mesh.attributes)
      {
        QRhiBuffer* qb = nullptr;
        std::int64_t byte_size = 0;
        if(attr.binding >= 0 && attr.binding < int(mesh.input.size()))
        {
          const int bufIdx = mesh.input[attr.binding].buffer;
          if(bufIdx >= 0 && bufIdx < int(mesh.buffers.size()))
          {
            if(auto* gpu = ossia::get_if<ossia::geometry::gpu_buffer>(
                   &mesh.buffers[bufIdx].data))
            {
              qb = static_cast<QRhiBuffer*>(gpu->handle);
              byte_size = gpu->byte_size;
            }
          }
        }

        auto& slot = m_harvest->attributes.emplace_back();
        slot.name = std::string(ossia::geometry::display_name(attr));
        // A null handle leaves rb.data empty — surfaced by the driver as a
        // hard error ("empty readback is a failure"), not silently skipped.
        if(qb && byte_size > 0)
          // The size parameter is `int` on Qt 6.4 and `quint32` from 6.6; an
          // explicit narrowing cast from the int64 byte counts satisfies both
          // (and keeps -Wshorten-64-to-32 quiet), since each converts to the
          // other implicitly.
          ensureBatch()->readBackBuffer(qb, 0, quint32(byte_size), &slot.rb);
      }

      // Auxiliary buffers riding the geometry (the CSF publishes both its
      // geometry-level AUXILIARY SSBOs and its standalone storage buffers
      // here, by name).
      for(const auto& aux : mesh.auxiliary)
      {
        QRhiBuffer* qb = nullptr;
        if(aux.buffer >= 0 && aux.buffer < int(mesh.buffers.size()))
          if(auto* gpu = ossia::get_if<ossia::geometry::gpu_buffer>(
                 &mesh.buffers[aux.buffer].data))
            qb = static_cast<QRhiBuffer*>(gpu->handle);

        auto& slot = m_harvest->auxiliaries.emplace_back();
        slot.name = aux.name;
        if(qb && aux.byte_size > 0)
          ensureBatch()->readBackBuffer(
              qb, quint32(aux.byte_offset), quint32(aux.byte_size), &slot.rb);
      }
    }

    // --- Storage-buffer edges: one readback per edge whose SOURCE port is a
    // Types::Buffer output, resolved through the exported
    // RenderList::bufferForInput => source renderer's bufferForOutput
    // (RenderedCSFNode returns its m_outStorageBuffers entry). Geometry-
    // sourced edges resolve to an empty BufferView there and are skipped —
    // they were handled through the pushed spec above.
    for(score::gfx::Edge* e : p.edges)
    {
      if(!e->source || e->source->type != score::gfx::Types::Buffer)
        continue;

      score::gfx::BufferView view = renderer.bufferForInput(*e);

      // Name by the source output port's index, for stable identification
      // when several storage outputs fan into this port.
      int srcPortIdx = -1;
      if(e->source->node)
      {
        const auto& outs = e->source->node->output;
        for(std::size_t i = 0; i < outs.size(); ++i)
          if(outs[i] == e->source)
          {
            srcPortIdx = int(i);
            break;
          }
      }

      auto& slot = m_harvest->storage.emplace_back();
      slot.name = "storage:" + std::to_string(srcPortIdx);
      if(view.handle && view.byte_size > 0)
        ensureBatch()->readBackBuffer(
            view.handle, quint32(view.byte_offset), quint32(view.byte_size),
            &slot.rb);
    }
  }

private:
  std::shared_ptr<BufferSinkNode::Harvest> m_harvest;
};

inline score::gfx::OutputNodeRenderer*
BufferSinkNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new BufferSinkRenderer{*this, harvest};
}

// -----------------------------------------------------------------------------
// GfxPipeline composition.
// -----------------------------------------------------------------------------

/// Register a BufferSinkNode in a GfxPipeline. The node lives in the
/// pipeline's addNode() index space; keep the returned pointer to wire ports
/// (sink->sinkInput()), to render, and to harvest.
struct AttachedBufferSink
{
  int nodeIndex{-1};
  BufferSinkNode* node{};
};

inline AttachedBufferSink attach_buffer_sink(GfxPipeline& p)
{
  auto owned = std::make_unique<BufferSinkNode>();
  BufferSinkNode* raw = owned.get();
  const int idx = p.addNode(std::move(owned));
  if(idx < 0)
    return {};
  return {idx, raw};
}

/// Pump `frames` frames through a pipeline that carries a BufferSinkNode.
/// GfxPipeline::render delivers the per-frame Timings Message to every node
/// (BufferSinkNode's inherited Node::process is a no-op) and renders the
/// BackgroundNode sinks, but knows nothing about our sink — so each frame is
/// one p.render(1) followed by an explicit sink render. After the LAST frame
/// the sink's harvest holds that frame's bytes.
inline void
render_with_buffer_sink(GfxPipeline& p, BufferSinkNode& sink, int frames)
{
  for(int f = 0; f < frames; ++f)
  {
    p.render(1);
    sink.render();
  }
}

// -----------------------------------------------------------------------------
// One-call driver: CSF chain -> BufferSinkNode -> exact bytes.
// -----------------------------------------------------------------------------

/// One named readback, as plain bytes (already completed / CPU-side).
struct NamedBytes
{
  std::string name;
  QByteArray bytes;
};

/// Result of render_csf_buffer_readback. Never thrown — run Catch2 macros on
/// it AFTER run_in_gui_app returns (same rule as IsfResult, see Gfx.hpp).
struct CsfBufferResult
{
  bool skipped = false;      // no RHI / no compute / no non-uniform readback
  std::string skip_reason;

  std::string error;         // non-empty => real failure (incl. empty readback)
  std::string backend;       // actual QRhi backend name when known

  int vertices = 0;          // from the pushed geometry (0 when none wired)
  int instances = 0;
  bool geometry_seen = false;

  std::vector<NamedBytes> attributes;  // geometry attribute SSBOs, by name
  std::vector<NamedBytes> auxiliaries; // aux/storage buffers riding the geometry
  std::vector<NamedBytes> storage;     // Buffer-output readbacks, wiring order

  const NamedBytes* attribute(std::string_view name) const noexcept
  {
    for(const auto& a : attributes)
      if(a.name == name)
        return &a;
    return nullptr;
  }
  const NamedBytes* auxiliary(std::string_view name) const noexcept
  {
    for(const auto& a : auxiliaries)
      if(a.name == name)
        return &a;
    return nullptr;
  }
};

/// Reinterpret a readback as tightly-packed little-endian float32 / int32
/// (what std430 scalar/vecN arrays are on every supported backend).
inline std::vector<float> as_floats(const QByteArray& b)
{
  std::vector<float> v(std::size_t(b.size()) / sizeof(float));
  if(!v.empty())
    std::memcpy(v.data(), b.constData(), v.size() * sizeof(float));
  return v;
}
inline std::vector<int32_t> as_ints(const QByteArray& b)
{
  std::vector<int32_t> v(std::size_t(b.size()) / sizeof(int32_t));
  if(!v.empty())
    std::memcpy(v.data(), b.constData(), v.size() * sizeof(int32_t));
  return v;
}

/// Render a linear CSF chain offscreen with NO raster/image consumer and read
/// the final stage's geometry attribute SSBOs, its auxiliary/storage buffers,
/// and every Types::Buffer output back byte-exactly.
///
///   csfChain[0]    : a CSF (usually a write_only geometry producer)
///   csfChain[i]    : reads csfChain[i-1]'s Geometry output on its first
///                    Geometry input (read_write / filter-style CSFs)
///   csfChain.back(): its Geometry output (if any) and each of its Buffer
///                    outputs feed the sink.
///
/// `frames` >= 1; the harvest is the LAST frame's bytes (deterministic for
/// time-independent shaders). `size` shapes the sink's offscreen state, not
/// the buffers.
inline CsfBufferResult render_csf_buffer_readback(
    score::gfx::GraphicsApi backend, std::vector<QString> csfChain,
    QSize size = {64, 64}, int frames = 3)
{
  CsfBufferResult r;
  r.backend = backend_name(backend);

  if(csfChain.empty())
  {
    r.error = "render_csf_buffer_readback: no CSF paths given";
    return r;
  }

  // 1. Backend availability — identical semantics to render_isf_chain.
  {
    std::string probed;
    if(!probe_api(backend, probed))
    {
      r.skipped = true;
      r.skip_reason = std::string("RHI backend '") + backend_name(backend)
                      + "' cannot initialize on this machine (no driver/ICD, "
                        "wrong platform, or a legacy/unsupported GL context)";
      return r;
    }
  }

  // 2. Build the pipeline: CSF chain -> buffer sink.
  GfxPipeline p;

  std::vector<int> csf;
  for(const auto& path : csfChain)
  {
    const int idx = p.addCsf(path);
    if(idx < 0)
    {
      r.error = "CSF build failed (" + path.toStdString() + "): " + p.error();
      return r;
    }
    csf.push_back(idx);
  }

  auto sink = attach_buffer_sink(p);
  if(!sink.node)
  {
    r.error = "could not attach BufferSinkNode: " + p.error();
    return r;
  }

  // Chain the geometry stages: stage[i].geoOut -> stage[i+1].geoIn.
  for(std::size_t i = 0; i + 1 < csf.size(); ++i)
  {
    auto* gout = p.geometryOut(csf[i], 0);
    auto* gin = p.geometryIn(csf[i + 1], 0);
    if(!gout || !gin)
    {
      r.error = "CSF chain stage " + std::to_string(i)
                + " is missing a Geometry port";
      return r;
    }
    p.wire(gout, gin);
  }

  // Final stage -> sink. At least one of {geometry, buffer} must be wired,
  // otherwise nothing makes the CSF reachable and nothing can be read.
  const int last = csf.back();
  bool wired = false;
  if(auto* gout = p.geometryOut(last, 0))
  {
    p.wire(gout, sink.node->sinkInput());
    wired = true;
  }
  for(int k = 0;; ++k)
  {
    auto* bout = p.bufferOut(last, k);
    if(!bout)
      break;
    p.wire(bout, sink.node->sinkInput());
    wired = true;
  }
  if(!wired)
  {
    r.error = "final CSF '" + csfChain.back().toStdString()
              + "' exposes neither a Geometry nor a Buffer output port";
    return r;
  }

  sink.node->setSize(size);

  // 3. Bring up the render lists. GfxPipeline::create only verifies ITS OWN
  //    BackgroundNode sinks (none here), so verify our sink's QRhi ourselves.
  if(!p.create(backend))
  {
    r.skipped = p.skipped();
    r.skip_reason = p.skipReason();
    r.error = p.error();
    return r;
  }
  auto state = sink.node->renderState();
  if(!state || !state->rhi)
  {
    r.skipped = true;
    r.skip_reason
        = "BufferSinkNode could not create a QRhi offscreen context headless "
          "(probe succeeded but device/offscreen allocation failed)";
    return r;
  }
  QRhi& rhi = *state->rhi;
  r.backend = rhi.backendName();

  if(!rhi.isFeatureSupported(QRhi::Compute))
  {
    r.skipped = true;
    r.skip_reason = r.backend + ": compute shaders unsupported on this device";
    return r;
  }
  if(!rhi.isFeatureSupported(QRhi::ReadBackNonUniformBuffer))
  {
    r.skipped = true;
    r.skip_reason
        = r.backend + ": QRhi::ReadBackNonUniformBuffer unsupported (cannot "
                      "read storage buffers back)";
    return r;
  }

  // 4. Render. The compute dispatch happens because the sink's input edges
  //    make the CSF reachable — no raster consumer, no BackgroundNode.
  render_with_buffer_sink(p, *sink.node, frames);

  // 5. Harvest — bytes are complete after endOffscreenFrame inside render().
  const auto& h = *sink.node->harvest;
  if(h.readback_unsupported)
  {
    // Should have been caught by the feature check above; belt and braces.
    r.skipped = true;
    r.skip_reason = r.backend + ": non-uniform buffer readback unsupported";
    return r;
  }

  r.vertices = h.vertices;
  r.instances = h.instances;
  r.geometry_seen = h.geometry_seen;

  const auto take = [&r](const std::deque<BufferSinkNode::NamedReadback>& in,
                         std::vector<NamedBytes>& out, const char* what) {
    for(const auto& nr : in)
    {
      if(nr.rb.data.isEmpty() && r.error.empty())
        r.error = std::string(what) + " readback '" + nr.name
                  + "' came back empty on a backend that claims support";
      out.push_back(NamedBytes{nr.name, nr.rb.data});
    }
  };
  take(h.attributes, r.attributes, "attribute");
  take(h.auxiliaries, r.auxiliaries, "auxiliary");
  take(h.storage, r.storage, "storage");

  // A wired geometry edge whose compute pass never pushed a mesh is a
  // dispatch failure, not a skip.
  if(p.geometryOut(last, 0) && !h.geometry_seen && r.error.empty())
    r.error = "geometry output wired but no geometry_spec reached the sink — "
              "the compute pass did not dispatch / push";
  if(r.attributes.empty() && r.auxiliaries.empty() && r.storage.empty()
     && r.error.empty())
    r.error = "no buffers were harvested at all";

  return r;
}

}
